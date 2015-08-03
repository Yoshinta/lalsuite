/*
 *  LALInferenceMCMC.c:  Bayesian Followup, MCMC algorithm.
 *
 *  Copyright (C) 2009, 2012 Ilya Mandel, Vivien Raymond, Christian
 *  Roever, Marc van der Sluys, John Veitch, Will M. Farr, and Ben Farr
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <lal/LALInspiral.h>
#include <lal/DetResponse.h>
#include <lal/SeqFactories.h>
#include <lal/Date.h>
#include <lal/VectorOps.h>
#include <lal/TimeFreqFFT.h>
#include <lal/GenerateInspiral.h>
#include <lal/TimeDelay.h>
#include <lalapps.h>
#include <mpi.h>
#include <lal/LALInference.h>
#include "LALInferenceMCMCSampler.h"
#include <lal/LALInferencePrior.h>
#include <lal/LALInferenceLikelihood.h>
#include <lal/LALInferenceTemplate.h>
#include <lal/LALInferenceProposal.h>
#include <lal/LIGOLwXMLRead.h>
#include <lal/LIGOLwXMLInspiralRead.h>
#include <lal/LALInferenceReadData.h>
#include <sys/time.h>

#include <LALAppsVCSInfo.h>
#include <lal/LALStdlib.h>

#define PROGRAM_NAME "LALInferenceMCMCSampler.c"
#define CVS_ID_STRING "$Id$"
#define CVS_REVISION "$Revision$"
#define CVS_SOURCE "$Source$"
#define CVS_DATE "$Date$"
#define CVS_NAME_STRING "$Name$"

const char *const clusteredKDEProposalName = "ClusteredKDEProposal";

static void
thinDifferentialEvolutionPoints(LALInferenceThreadState *thread) {
    size_t i;
    size_t newSize;

    /* Delete all the even-index points. */
    for (i = 0; i < thread->differentialPointsLength; i += 2) {
        LALInferenceClearVariables(thread->differentialPoints[i]);
        XLALFree(thread->differentialPoints[i]);
        thread->differentialPoints[i] = NULL;
    }

    /* Copy the odd points into the first part of the array. */
    for (i = 1; i < thread->differentialPointsLength; i += 2) {
        thread->differentialPoints[i/2] = thread->differentialPoints[i];
        thread->differentialPoints[i] = NULL;
    }

    newSize = thread->differentialPointsLength / 2;

    /* Now shrink the buffer down. */
    thread->differentialPoints = XLALRealloc(thread->differentialPoints, 2*newSize*sizeof(LALInferenceVariables *));
    thread->differentialPointsSize = 2*newSize;
    thread->differentialPointsLength = newSize;
    thread->differentialPointsSkip *= 2;
}

static void
accumulateDifferentialEvolutionSample(LALInferenceThreadState *thread, size_t buffer_limit) {
    if (thread->differentialPointsSize == thread->differentialPointsLength) {
        size_t newSize = thread->differentialPointsSize*2;

        if (buffer_limit < newSize) {
            /* Then thin, and record sample. */
            thinDifferentialEvolutionPoints(thread);
            return accumulateDifferentialEvolutionSample(thread, buffer_limit);
        } else {
            thread->differentialPoints = XLALRealloc(thread->differentialPoints, newSize*sizeof(LALInferenceVariables *));
            thread->differentialPointsSize = newSize;
        }
    }

    thread->differentialPoints[thread->differentialPointsLength] = XLALCalloc(1, sizeof(LALInferenceVariables));
    LALInferenceCopyVariables(thread->currentParams, thread->differentialPoints[thread->differentialPointsLength]);

    thread->differentialPointsLength += 1;
}

static void
resetDifferentialEvolutionBuffer(LALInferenceThreadState *thread) {
    size_t i;

    for (i = 0; i < thread->differentialPointsLength; i++) {
        LALInferenceClearVariables(thread->differentialPoints[i]);
        XLALFree(thread->differentialPoints[i]);
        thread->differentialPoints[i] = NULL;
    }

    thread->differentialPoints = XLALRealloc(thread->differentialPoints, sizeof(LALInferenceVariables *));
    thread->differentialPointsLength = 0;
    thread->differentialPointsSize = 1;
    thread->differentialPointsSkip = LALInferenceGetINT4Variable(thread->proposalArgs, "de_skip");
}


void PTMCMCAlgorithm(struct tagLALInferenceRunState *runState) {
    INT4 t=0; //indexes for for() loops
    INT4 *step;
    INT4 runComplete = 0;
    REAL8 timestamp_epoch=0.0;
    INT4 MPIrank, MPIsize;
    LALStatus status;
    LALInferenceVariables *algorithm_params;
    LALInferenceVariableItem *ptr;
    LALInferenceThreadState *thread;

    memset(&status, 0, sizeof(status));

    MPI_Comm_size(MPI_COMM_WORLD, &MPIsize);
    MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);

    algorithm_params = runState->algorithmParams;

    /* Get all algorithm params */
    INT4 n_local_threads = runState->nthreads;
    INT4 nPar = LALInferenceGetVariableDimensionNonFixed(runState->threads[0]->currentParams);
    INT4 Niter = LALInferenceGetINT4Variable(algorithm_params, "nsteps");
    INT4 Neff = LALInferenceGetINT4Variable(algorithm_params, "neff");
    INT4 Nskip = LALInferenceGetINT4Variable(algorithm_params, "skip");
    INT4 de_buffer_limit = LALInferenceGetINT4Variable(algorithm_params, "de_buffer_limit");
    INT4 randomseed = LALInferenceGetINT4Variable(algorithm_params, "random_seed");

    INT4 verbose = LALInferenceGetINT4Variable(algorithm_params, "verbose");
    INT4 propVerbose = LALInferenceGetINT4Variable(algorithm_params, "prop_verbose");
    INT4 propTrack = LALInferenceGetINT4Variable(algorithm_params, "prop_track");
    INT4 tempVerbose = LALInferenceGetINT4Variable(algorithm_params, "temp_verbose");
    INT4 adaptVerbose = LALInferenceGetINT4Variable(algorithm_params, "adapt_verbose");
    INT4 benchmark = LALInferenceGetINT4Variable(algorithm_params, "benchmark");

    /* Clustered-KDE proposal updates */
    INT4 kde_update_start = 200;  // rough number of effective samples to start KDE updates

    /* proposal will be updated 5 times per decade, so this interval will change */
    INT4 *kde_update_interval = XLALCalloc(n_local_threads, sizeof(INT4));
    INT4 *last_kde_update = XLALCalloc(n_local_threads, sizeof(INT4)); // effective sample size at last KDE update

    INT4 diffEvo = 1;
    if (runState->threads[0]->differentialPoints == NULL)
        diffEvo = 0;

    /* Adaptation settings */
    INT4 no_adapt = LALInferenceGetINT4Variable(runState->algorithmParams, "no_adapt");
    INT4 adaptTau = LALInferenceGetINT4Variable(runState->algorithmParams, "adaptTau");
    INT4 adaptLength = LALInferenceGetINT4Variable(runState->algorithmParams, "adaptLength"); // Number of iterations to adapt before turning off

    /* File outputs */
    FILE *statfile = NULL;
    FILE *swapfile = NULL;
    char statfilename[256];
    char swapfilename[256];

    if (tempVerbose) {
        for (t = 0; t < n_local_threads; t++) {
            sprintf(swapfilename, "PTMCMC.tempswaps.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
            swapfile = fopen(swapfilename, "w");

            fprintf(swapfile,
                "cycle\tlog(chain_swap)\tlow_temp_likelihood\thigh_temp_likelihood\tswap_accepted\n");

            fclose(swapfile);
        }
    }

    if (adaptVerbose) {
        for (t = 0; t < n_local_threads; t++) {
            thread = runState->threads[t];
            if (!no_adapt) {
                sprintf(statfilename, "PTMCMC.statistics.%u.%2.2d",
                        randomseed, n_local_threads*MPIrank+t);
                statfile = fopen(statfilename, "w");

                fprintf(statfile, "cycle\ts_gamma");
                ptr = thread->currentParams->head;
                while (ptr != NULL) {
                    if (ptr->vary != LALINFERENCE_PARAM_FIXED) {
                        fprintf(statfile, "\tsigma_%s",
                                LALInferenceTranslateInternalToExternalParamName(ptr->name));
                    }
                    ptr = ptr->next;
                }
                ptr = thread->currentParams->head;
                while ( ptr != NULL) {
                    if (ptr->vary != LALINFERENCE_PARAM_FIXED) {
                        fprintf(statfile, "\tPaccept_%s",
                                LALInferenceTranslateInternalToExternalParamName(ptr->name));
                    }
                    ptr=ptr->next;
                }
                fprintf(statfile,"\n");

                fclose(statfile);
            }
        }
    }

    /* This is mangling currentParams */
    FILE **threadoutputs = LALInferencePrintPTMCMCHeadersOrResume(runState);
    if (MPIrank == 0)
        LALInferencePrintInjectionSample(runState);

    if (benchmark)
        timestamp_epoch = LALInferenceGetREAL8Variable(runState->algorithmParams, "timestamp_epoch");

    /* Print run details */
    if (MPIrank == 0) {
        thread = runState->threads[0];
        if (verbose) {
            printf("\nParallel Behavior:\n");
            if (!no_adapt)
                printf(" Adapting with decay power %i for %i iterations after max log(L) increases by nParams/2 (%1.2f).\n", adaptTau, adaptLength, (double)nPar/2.0);
            else
                printf(" Adaptation off.\n");
            if (Neff != Niter)
                printf(" Collecting %i effective samples.\n", Neff);

            printf("\nPTMCMCAlgorithm(); starting parameter values:\n");
            LALInferencePrintVariables(thread->currentParams);
            printf(" MCMC iteration: 0\t");
            printf("%f\t", thread->currentLikelihood - thread->nullLikelihood);
            printf("\n");
        }

        /* Print to file the contents of model->freqhPlus. */
        if (LALInferenceGetProcParamVal(runState->commandLine, "--data-dump"))
            LALInferenceDataDump(runState->data, thread->model);
    }

    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);

    // iterate:
    step = (INT4 *)LALInferenceGetVariable(runState->algorithmParams, "step");
    while (!runComplete) {
        /* Increment iteration counter */
        (*step)++;

        #pragma omp parallel for private(thread)
        for (t = 0; t < n_local_threads; t++) {
            FILE *propstatfile = NULL;
            FILE *proptrackfile = NULL;
            char propstatfilename[256];
            char proptrackfilename[256];
            struct timeval tv;
            REAL8 timestamp=-1.0;

            thread = runState->threads[t];
            INT4 adapting = LALInferenceGetINT4Variable(thread->proposalArgs, "adapting");

            if (!no_adapt)
                LALInferenceAdaptation(thread, *step);

            //ACL calculation
            INT4 iEff = 0;
            if (*step % (100*Nskip) == 0) {
                if (adapting)
                    iEff = 0;
                else
                    iEff = LALInferenceComputeEffectiveSampleSize(thread);
            }

            if (MPIrank == 0 && t == 0 && iEff > Neff) {
                fprintf(stdout,"Thread %i has %i effective samples. Stopping...\n", MPIrank, iEff);
                runComplete = 1;          // Sampling is done!
            }

            mcmc_step(runState, thread); //evolve the chain at temperature ladder[t]

            if (propVerbose)
                LALInferenceTrackProposalAcceptance(thread);

            /* Print proposal tracking headers now that the proposal cycle should be built. */
            if (*step==1){
                if (propVerbose) {
                    sprintf(propstatfilename, "PTMCMC.propstats.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
                    propstatfile = fopen(propstatfilename, "w");

                    fprintf(propstatfile, "cycle\t");
                    LALInferencePrintProposalStatsHeader(propstatfile, thread->cycle);
                    fclose(propstatfile);

                    if (propTrack) {
                        sprintf(proptrackfilename, "PTMCMC.proptrack.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
                        proptrackfile = fopen(proptrackfilename, "w");

                        fprintf(proptrackfile, "cycle\t");
                        LALInferencePrintProposalTrackingHeader(proptrackfile, thread->currentParams);
                        fclose(proptrackfile);
                    }
                }
            }

            if ((*step % Nskip) == 0) {
                /* Update clustered-KDE proposal every time the buffer is expanded */
                if (LALInferenceGetProcParamVal(runState->commandLine, "--proposal-kde")
                    && (iEff > kde_update_start)
                    && (((iEff - last_kde_update[t]) > kde_update_interval[t]) ||
                      ((last_kde_update[t] - iEff) > kde_update_interval[t]))) {
                    LALInferenceSetupClusteredKDEProposalFromDEBuffer(thread);

                    /* Update 5 times each decade.  This keeps hot chains (with lower ACLs) under control */
                    kde_update_interval[t] = 2 * ((INT4) pow(10.0, floor(log10((REAL8) iEff))));

                    last_kde_update[t] = iEff;
                }

                if (diffEvo && (*step % thread->differentialPointsSkip == 0))
                    accumulateDifferentialEvolutionSample(thread, de_buffer_limit);

                if (benchmark) {
                    gettimeofday(&tv, NULL);
                    timestamp = tv.tv_sec + tv.tv_usec/1E6 - timestamp_epoch;
                }

                LALInferencePrintMCMCSample(thread, runState->data, *step, timestamp, threadoutputs[t]);

                if (adaptVerbose && !no_adapt) {
                    REAL8 s_gamma = 0.0;

                    sprintf(statfilename, "PTMCMC.statistics.%u.%2.2d",
                            randomseed, n_local_threads*MPIrank+t);
                    statfile = fopen(statfilename, "a");

                    fprintf(statfile,"%d\t",*step);

                    if (LALInferenceCheckVariable(thread->proposalArgs, "s_gamma"))
                        s_gamma = LALInferenceGetREAL8Variable(thread->proposalArgs, "s_gamma");
                    fprintf(statfile,"%f\t",s_gamma);

                    for(LALInferenceVariableItem *item=thread->currentParams->head;item;item=item->next){
                        char tmpname[MAX_STRLEN]="";
                        sprintf(tmpname,"%s_%s",item->name,ADAPTSUFFIX);
                        REAL8 *sigma=(REAL8 *)LALInferenceGetVariable(thread->proposalArgs,tmpname);
                        fprintf(statfile,"%g\t",*sigma);
                    }

                    for(LALInferenceVariableItem *item=thread->currentParams->head;item;item=item->next){
                        char tmpname[MAX_STRLEN]="";
                        sprintf(tmpname,"%s_%s",item->name,ACCEPTSUFFIX);
                        REAL8 *naccepted=(REAL8 *)LALInferenceGetVariable(thread->proposalArgs,tmpname);
                        sprintf(tmpname,"%s_%s",item->name,PROPOSEDSUFFIX);
                        REAL8 *nproposed=(REAL8 *)LALInferenceGetVariable(thread->proposalArgs,tmpname);
                        fprintf(statfile,"%f\t",*naccepted/( *nproposed==0 ? 1.0 : *nproposed ));
                    }

                    fprintf(statfile,"\n");
                    fclose(statfile);
                }

                if (propVerbose){
                    sprintf(propstatfilename, "PTMCMC.propstats.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
                    propstatfile = fopen(propstatfilename, "w");
                    fprintf(propstatfile, "%d\t", *step);
                    LALInferencePrintProposalStats(propstatfile, thread->cycle);
                    fclose(propstatfile);
                }

                if (propTrack) {
                    REAL8 logProposalRatio = LALInferenceGetREAL8Variable(thread->proposalArgs, "logProposalRatio");
                    sprintf(proptrackfilename, "PTMCMC.proptrack.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
                    proptrackfile = fopen(proptrackfilename, "w");
                    fprintf(proptrackfile, "%d\t", *step);
                    LALInferencePrintProposalTracking(proptrackfile, thread->cycle, thread->preProposalParams, thread->proposedParams, logProposalRatio, thread->accepted);
                    fclose(proptrackfile);
                }
            }
        }

        /* Open swap file if going verbose */
        if (tempVerbose) {
            sprintf(swapfilename, "PTMCMC.tempswaps.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
            swapfile = fopen(swapfilename, "w");
        }

        /* Excute swap proposal. */
        runState->parallelSwap(runState, *step, swapfile);

        if (tempVerbose)
            fclose(swapfile);

        /* Broadcast the root's decision on run completion */
        MPI_Bcast(&runComplete, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (*step > Niter)
            runComplete=1;
    }// while (!runComplete)
}

void mcmc_step(LALInferenceRunState *runState, LALInferenceThreadState *thread) {
    // Metropolis-Hastings sampler.
    INT4 MPIrank;
    REAL8 logPriorCurrent, logPriorProposed;
    REAL8 logLikelihoodCurrent, logLikelihoodProposed;
    REAL8 logProposalRatio = 0.0;  // = log(P(backward)/P(forward))
    REAL8 logAcceptanceProbability;
    REAL8 targetAcceptance = 0.234;

    MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);

    INT4 outputSNRs = LALInferenceGetINT4Variable(runState->algorithmParams, "output_snrs");
    INT4 propTrack = LALInferenceGetINT4Variable(runState->algorithmParams, "prop_track");

    // current values:
    logPriorCurrent = thread->currentPrior;
    logLikelihoodCurrent = thread->currentLikelihood;

    // generate proposal:
    logProposalRatio = thread->proposal(thread, thread->currentParams, thread->proposedParams);

    // compute prior & likelihood:
    logPriorProposed = runState->prior(runState, thread->proposedParams, thread->model);
    if (logPriorProposed > -DBL_MAX)
        logLikelihoodProposed = runState->likelihood(thread->proposedParams, runState->data, thread->model);
    else
        logLikelihoodProposed = -DBL_MAX;

    if (propTrack)
        LALInferenceCopyVariables(thread->currentParams, thread->preProposalParams);

    // determine acceptance probability:
    logAcceptanceProbability = (1.0/thread->temperature)*(logLikelihoodProposed - logLikelihoodCurrent)
                                + (logPriorProposed - logPriorCurrent)
                                + logProposalRatio;

    // accept/reject:
    if ((logAcceptanceProbability > 0)
        || (log(gsl_rng_uniform(thread->GSLrandom)) < logAcceptanceProbability)) {   //accept
        LALInferenceCopyVariables(thread->proposedParams, thread->currentParams);
        thread->currentLikelihood = logLikelihoodProposed;
        thread->currentPrior = logPriorProposed;

        /* Calculate SNR if requested, and not already calculated by prior */
        LALInferenceIFOData *ifoPtr = runState->data;
        INT4 ifo = 0;
        if (outputSNRs) {
            if (thread->model->SNR == 0.0)
                LALInferenceNetworkSNR(thread->currentParams, runState->data, thread->model);
            thread->currentSNR = thread->model->SNR;
            while (ifoPtr) {
                thread->currentIFOSNRs[ifo] = thread->model->ifo_SNRs[ifo];
                ifo++;
                ifoPtr = ifoPtr->next;
            }
        }

        if (thread->model->ifo_loglikelihoods != NULL) {
            ifoPtr = runState->data;
            ifo = 0;
            while (ifoPtr) {
                thread->currentIFOLikelihoods[ifo] = thread->model->ifo_loglikelihoods[ifo];
                ifo++;
                ifoPtr = ifoPtr->next;
            }
        }

        thread->acceptanceCount++;
        thread->accepted = 1;
    } else {
        thread->accepted = 0;
    }

    LALInferenceUpdateAdaptiveJumps(thread, targetAcceptance);

    return;
}


//-----------------------------------------
// Swap routines:
//-----------------------------------------
void LALInferencePTswap(LALInferenceRunState *runState, INT4 i, FILE *swapfile) {
    INT4 MPIrank, MPIsize;
    MPI_Status MPIstatus;
    INT4 nPar, adjNPar, n_local_threads, ntemps, Tskip;
    INT4 low;
    INT4 ind, cold_ind, hot_ind;
    INT4 cold_rank, hot_rank;
    INT4 swapAccepted;
    INT4 *cold_inds;
    REAL8 adjCurrentLikelihood, adjCurrentPrior;
    REAL8 logThreadSwap, temp_prior, temp_like, cold_temp;
    LALInferenceThreadState *cold_thread = runState->threads[0];
    LALInferenceThreadState *hot_thread;
    LALInferenceVariables *temp_params;

    MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
    MPI_Comm_size(MPI_COMM_WORLD, &MPIsize);

    n_local_threads = runState->nthreads;
    Tskip = LALInferenceGetINT4Variable(runState->algorithmParams, "tskip");

    ntemps = MPIsize*n_local_threads;
    cold_inds = XLALCalloc(ntemps-1, sizeof(INT4));

    /* Return if no swap should be proposed */
    if((i % Tskip) != 0 || ntemps<2) return;

    /* Have the root process choose a random order of ladder swaps and share it */
    if (MPIrank == 0) {
        for (low = 0; low < ntemps-1; low++)
            cold_inds[low] = low;

        gsl_ran_shuffle(runState->GSLrandom, cold_inds, ntemps-1, sizeof(INT4));
    }

    MPI_Bcast(cold_inds, ntemps-1, MPI_INT, 0, MPI_COMM_WORLD);

    for (ind=0; ind < ntemps-1; ind++) {
        cold_ind = cold_inds[ind];
        hot_ind = cold_ind+1;

        /* Check if cold chain is handled by this thread */
        cold_rank = cold_ind/n_local_threads;
        hot_rank = hot_ind/n_local_threads;

        if (cold_rank == hot_rank) {
            if (MPIrank == cold_rank) {
                cold_thread = runState->threads[cold_ind % n_local_threads];
                hot_thread = runState->threads[hot_ind % n_local_threads];

                /* Determine if swap is accepted and tell the other chain */
                logThreadSwap = 1.0/cold_thread->temperature - 1.0/hot_thread->temperature;
                logThreadSwap *= hot_thread->currentLikelihood - cold_thread->currentLikelihood;

                if ((logThreadSwap > 0) || (log(gsl_rng_uniform(runState->GSLrandom)) < logThreadSwap ))
                    swapAccepted = 1;
                else
                    swapAccepted = 0;

                /* Print to file if verbose is chosen */
                if (swapfile != NULL) {
                    fprintf(swapfile, "%d\t%f\t%f\t%f\t%f\t%f\t%i\n",
                            i, cold_thread->temperature, hot_thread->temperature,
                            logThreadSwap, cold_thread->currentLikelihood,
                            hot_thread->currentLikelihood, swapAccepted);
                    fflush(swapfile);
                }

                if (swapAccepted) {
                    temp_params = hot_thread->currentParams;
                    temp_prior = hot_thread->currentPrior;
                    temp_like = hot_thread->currentLikelihood;

                    hot_thread->currentParams = cold_thread->currentParams;
                    hot_thread->currentPrior = cold_thread->currentPrior;
                    hot_thread->currentLikelihood = cold_thread->currentLikelihood;

                    cold_thread->currentParams = temp_params;
                    cold_thread->currentPrior = temp_prior;
                    cold_thread->currentLikelihood = temp_like;
                }
            }
        } else {
            if (MPIrank == cold_rank) {
                cold_thread = runState->threads[cold_ind % n_local_threads];

                /* Send current likelihood for swap proposal */
                MPI_Send(&(cold_thread->temperature), 1, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD);
                MPI_Send(&(cold_thread->currentLikelihood), 1, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD);

                /* Determine if swap was accepted */
                MPI_Recv(&swapAccepted, 1, MPI_INT, hot_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);

                /* Perform Swap */
                if (swapAccepted) {
                    /* Set new likelihood */
                    MPI_Recv(&adjCurrentLikelihood, 1, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);
                    cold_thread->currentLikelihood = adjCurrentLikelihood;

                    /* Exchange current prior values */
                    MPI_Send(&(cold_thread->currentPrior), 1, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD);
                    MPI_Recv(&adjCurrentPrior, 1, MPI_DOUBLE, hot_rank, 0, MPI_COMM_WORLD, &MPIstatus);
                    cold_thread->currentPrior = adjCurrentPrior;

                    /* Package and send parameters */
                    nPar = LALInferenceGetVariableDimensionNonFixed(cold_thread->currentParams);
                    MPI_Send(&nPar, 1, MPI_INT, hot_rank, PT_COM, MPI_COMM_WORLD);
                    MPI_Recv(&adjNPar, 1, MPI_INT, hot_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);

                    REAL8 *parameters = XLALMalloc(nPar * sizeof(REAL8));
                    LALInferenceCopyVariablesToArray(cold_thread->currentParams, parameters);
                    MPI_Send(parameters, nPar, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD);

                    /* Recieve and unpack parameters */
                    REAL8 *adjParameters = XLALMalloc(adjNPar * sizeof(REAL8));
                    MPI_Recv(adjParameters, adjNPar, MPI_DOUBLE, hot_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);
                    LALInferenceCopyArrayToVariables(adjParameters, cold_thread->currentParams);

                    XLALFree(parameters);
                    XLALFree(adjParameters);
                }
            } else if (MPIrank == hot_rank) {
                hot_thread = runState->threads[hot_ind % n_local_threads];

                MPI_Recv(&cold_temp, 1, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);

                /* Receive adjacent likelilhood */
                MPI_Recv(&adjCurrentLikelihood, 1, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);

                /* Determine if swap is accepted and tell the other chain */
                logThreadSwap = 1.0/cold_temp - 1.0/hot_thread->temperature;
                logThreadSwap *= hot_thread->currentLikelihood - adjCurrentLikelihood;
                if ((logThreadSwap > 0) || (log(gsl_rng_uniform(runState->GSLrandom)) < logThreadSwap ))
                    swapAccepted = 1;
                else
                    swapAccepted = 0;

                MPI_Send(&swapAccepted, 1, MPI_INT, cold_rank, PT_COM, MPI_COMM_WORLD);

                /* Print to file if verbose is chosen */
                if (swapfile != NULL) {
                    fprintf(swapfile, "%d%f\t%f\t\t%f\t%f\t%f\t%i\n",
                            i, cold_temp, hot_thread->temperature,
                            logThreadSwap, adjCurrentLikelihood,
                            hot_thread->currentLikelihood, swapAccepted);
                    fflush(swapfile);
                }

                /* Perform Swap */
                if (swapAccepted) {
                    /* Swap likelihoods */
                    MPI_Send(&(hot_thread->currentLikelihood), 1, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD);
                    hot_thread->currentLikelihood = adjCurrentLikelihood;

                    /* Exchange current prior values */
                    MPI_Recv(&adjCurrentPrior, 1, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);
                    MPI_Send(&(hot_thread->currentPrior), 1, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD);
                    hot_thread->currentPrior = adjCurrentPrior;

                    /* Package parameters */
                    nPar = LALInferenceGetVariableDimensionNonFixed(hot_thread->currentParams);
                    MPI_Recv(&adjNPar, 1, MPI_INT, cold_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);
                    MPI_Send(&nPar, 1, MPI_INT, cold_rank, PT_COM, MPI_COMM_WORLD);

                    REAL8 *parameters = XLALMalloc(nPar * sizeof(REAL8));
                    LALInferenceCopyVariablesToArray(hot_thread->currentParams, parameters);

                    /* Swap parameters */
                    REAL8 *adjParameters = XLALMalloc(adjNPar * sizeof(REAL8));
                    MPI_Recv(adjParameters, adjNPar, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD, &MPIstatus);
                    MPI_Send(parameters, nPar, MPI_DOUBLE, cold_rank, PT_COM, MPI_COMM_WORLD);

                    /* Unpack parameters */
                    LALInferenceCopyArrayToVariables(adjParameters, hot_thread->currentParams);

                    XLALFree(parameters);
                    XLALFree(adjParameters);
                }
            }
        }
    }

    XLALFree(cold_inds);

    return;
}


// UINT4 LALInferenceMCMCMCswap(LALInferenceRunState *runState, REAL8 *ladder, INT4 i, FILE *swapfile) {
//     INT4 MPIrank;
//     MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);
//     MPI_Status MPIstatus;
//     INT4 nPar = LALInferenceGetVariableDimensionNonFixed(runState->currentParams);
//     INT4 nChain = *(INT4*) LALInferenceGetVariable(runState->algorithmParams, "nChain");
//     INT4 Tskip = *(INT4*) LALInferenceGetVariable(runState->algorithmParams, "tempSkip");
//     REAL8 adjCurrentPrior;
//     REAL8 logChainSwap;
//     INT4 readyToSwap = 0;
//     UINT4 swapProposed=0;
//     INT4 swapAccepted=0;
//
//     REAL8 lowLikeLowParams = 0;
//     REAL8 highLikeHighParams = 0;
//     REAL8 lowLikeHighParams = 0;
//     REAL8 highLikeLowParams = 0;
//
//     LALInferenceParamVaryType fLowVary = LALINFERENCE_PARAM_FIXED;
//     LALInferenceVariables *adjCurrentParams = NULL;
//     adjCurrentParams = (LALInferenceVariables *)XLALCalloc(sizeof(LALInferenceVariables), 1);
//
//     /* If Tskip reached, then block until next chain in ladder is prepared to accept swap proposal */
//     if (((i % Tskip) == 0) && MPIrank < nChain-1) {
//         swapProposed = 1;
//         /* Send current likelihood for swap proposal */
//         MPI_Send(&(runState->currentLikelihood), 1, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD);
//
//         /* Fix fLow if it is being varied, to avoid swapping it */
//         if(LALInferenceCheckVariable(runState->currentParams,"fLow")) {
//             fLowVary = LALInferenceGetVariableVaryType(runState->currentParams,"fLow");
//             if(fLowVary != LALINFERENCE_PARAM_FIXED) {
//                 LALInferenceSetParamVaryType(runState->currentParams,"fLow",LALINFERENCE_PARAM_FIXED);
//                 nPar -= 1;
//             }
//         }
//
//         /* Prepare Variables structure to recieve adjacent parameters */
//         LALInferenceCopyVariables(runState->currentParams, adjCurrentParams);
//
//         /* Package, swap, and unpack parameters */
//         REAL8 *parameters = XLALMalloc(nPar * sizeof(REAL8));
//         REAL8 *adjParameters = XLALMalloc(nPar * sizeof(REAL8));
//         LALInferenceCopyVariablesToArray(runState->currentParams, parameters);
//         MPI_Send(parameters, nPar, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD);
//         MPI_Recv(adjParameters, nPar, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//         LALInferenceCopyArrayToVariables(adjParameters, adjCurrentParams);
//         XLALFree(parameters);
//         XLALFree(adjParameters);
//
//         /* Calculate likelihood at adjacent parameters and send */
//         lowLikeHighParams = runState->likelihood(adjCurrentParams, runState->data, runState->model);
//         MPI_Send(&lowLikeHighParams, 1, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD);
//
//         /* Determine if swap was accepted */
//         MPI_Recv(&swapAccepted, 1, MPI_INT, MPIrank+1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//
//         if (swapAccepted) {
//             /* Set new likelihood */
//             runState->currentLikelihood = lowLikeHighParams;
//
//             MPI_Send(&(runState->currentPrior), 1, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD);
//             MPI_Recv(&adjCurrentPrior, 1, MPI_DOUBLE, MPIrank+1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//             runState->currentPrior = adjCurrentPrior;
//
//             /* Set new parameters */
//             LALInferenceCopyVariables(adjCurrentParams,runState->currentParams);
//         }
//
//         /* Unfix fLow if it was originally unfixed */
//         if(fLowVary!=LALINFERENCE_PARAM_FIXED) {
//             LALInferenceSetParamVaryType(runState->currentParams,"fLow",fLowVary);
//             nPar += 1;
//         }
//
//         if (swapAccepted) {
//             /* Calculate prior at new values */
//             runState->currentPrior = runState->prior(runState, runState->currentParams, runState->model);
//         }
//     }
//
//     /* Check if next lower temperature is ready to swap */
//     else if (MPIrank > 0) {
//             MPI_Iprobe(MPIrank-1, PT_COM, MPI_COMM_WORLD, &readyToSwap, &MPIstatus);
//
//         /* Hotter chain decides acceptance */
//         if (readyToSwap) {
//             /* Receive adjacent likelilhood */
//             MPI_Recv(&lowLikeLowParams, 1, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//             highLikeHighParams = runState->currentLikelihood;
//
//             /* Fix fLow if it is being varied, to avoid swapping it */
//             if(LALInferenceCheckVariable(runState->currentParams,"fLow")) {
//                 fLowVary = LALInferenceGetVariableVaryType(runState->currentParams,"fLow");
//             if(fLowVary != LALINFERENCE_PARAM_FIXED) {
//                 LALInferenceSetParamVaryType(runState->currentParams,"fLow",LALINFERENCE_PARAM_FIXED);
//                 nPar -= 1;
//             }
//         }
//
//         /* Prepare Variables structure to recieve adjacent parameters */
//         LALInferenceCopyVariables(runState->currentParams, adjCurrentParams);
//
//         /* Package, swap, and unpack parameters */
//         REAL8 *parameters = XLALMalloc(nPar * sizeof(REAL8));
//         REAL8 *adjParameters = XLALMalloc(nPar * sizeof(REAL8));
//         LALInferenceCopyVariablesToArray(runState->currentParams, parameters);
//         MPI_Recv(adjParameters, nPar, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//         MPI_Send(parameters, nPar, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD);
//         LALInferenceCopyArrayToVariables(adjParameters, adjCurrentParams);
//         XLALFree(parameters);
//         XLALFree(adjParameters);
//
//         /* Calculate likelihood at adjacent parameters */
//         highLikeLowParams = runState->likelihood(adjCurrentParams, runState->data, runState->model);
//
//         /* Recieve likelihood from adjacent chain */
//         MPI_Recv(&lowLikeHighParams, 1, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//
//         /* Propose swap */
//         logChainSwap = (1./ladder[MPIrank-1])*(lowLikeHighParams-lowLikeLowParams)+(1./ladder[MPIrank])*(highLikeLowParams-highLikeHighParams);
//
//         if ((logChainSwap > 0) || (log(gsl_rng_uniform(runState->GSLrandom)) < logChainSwap )) {
//             swapAccepted = 1;
//         } else {
//             swapAccepted = 0;
//         }
//         MPI_Send(&swapAccepted, 1, MPI_INT, MPIrank-1, PT_COM, MPI_COMM_WORLD);
//
//         /* Print to file if verbose is chosen */
//         if (swapfile != NULL) {
//             fprintf(swapfile,"%d\t%f\t%f\t%f\t%i\n",i,logChainSwap,lowLikeLowParams,highLikeHighParams,swapAccepted);
//             fflush(swapfile);
//         }
//
//         if (swapAccepted) {
//             /* Swap likelihoods */
//             runState->currentLikelihood = highLikeLowParams;
//
//             /* Exchange current prior values (assumes prior functions are identical) */
//             MPI_Recv(&adjCurrentPrior, 1, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD, &MPIstatus);
//             MPI_Send(&(runState->currentPrior), 1, MPI_DOUBLE, MPIrank-1, PT_COM, MPI_COMM_WORLD);
//             runState->currentPrior = adjCurrentPrior;
//
//             /* Set new parameters */
//             LALInferenceCopyVariables(adjCurrentParams,runState->currentParams);
//         }
//
//         /* Unfix fLow if it was originally unfixed */
//         if(fLowVary!=LALINFERENCE_PARAM_FIXED) {
//             LALInferenceSetParamVaryType(runState->currentParams,"fLow",fLowVary);
//             nPar += 1;
//         }
//
//         if (swapAccepted) {
//             /* Calculate prior at new values */
//             runState->currentPrior = runState->prior(runState, runState->currentParams, runState->model);
//             }
//         }
//     }
//
//     /* Return values for colder chain: 0=nothing happened; 1=swap proposed, not accepted; 2=swap proposed & accepted */
//     if (swapProposed && swapAccepted)
//         swapProposed++;
//
//     LALInferenceClearVariables(adjCurrentParams);
//     XLALFree(adjCurrentParams);
//     return swapProposed;
// }


//-----------------------------------------
// Adaptation:
//-----------------------------------------
void LALInferenceAdaptation(LALInferenceThreadState *thread, INT4 cycle) {
    REAL8 s_gamma;

    INT4 nPar = LALInferenceGetVariableDimensionNonFixed(thread->currentParams);
    INT4 adaptLength = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptLength");

    /* if maximum logL has increased by more than nParam/2, restart it */
    INT4 adapting = LALInferenceGetINT4Variable(thread->proposalArgs, "adapting");
    INT4 adaptStart = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptStart");
    INT4 adaptTau = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptTau");
    INT4 adaptRestartBuffer = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptResetBuffer");
    REAL8 logLAtAdaptStart = LALInferenceGetREAL8Variable(thread->proposalArgs, "logLAtAdaptStart");

    if (thread->currentLikelihood > logLAtAdaptStart+(REAL8)nPar/2) {
        if (!adapting)
            fprintf(stdout, "Turning on adaptation for thread %u at iteration %u.\n", thread->id, cycle);
        LALInferenceAdaptationRestart(thread, cycle);
    } else if (adapting) {
        /* Turn off adaption after adaptLength steps without restarting */
        if ((cycle-adaptStart) > adaptLength) {
            adapting = 0;  //turn off adaptation
            LALInferenceSetVariable(thread->proposalArgs, "adapting", &adapting);
            LALInferenceRemoveVariable(thread->proposalArgs, "s_gamma");

            /* Clear differential buffer so that it contains only post-burnin samples */
            resetDifferentialEvolutionBuffer(thread);

            fprintf(stdout, "Ending adaptation for thread %u at iteration %u.\n", thread->id, cycle);

        /* Else set adaptation envelope */
        } else {
            s_gamma = LALInferenceAdaptationEnvelope(cycle, adaptStart, adaptLength,
                                                     adaptTau, adaptRestartBuffer);
            LALInferenceSetVariable(thread->proposalArgs, "s_gamma", &s_gamma);
        }
    }
}


//-----------------------------------------
// Restart adaptation:
//-----------------------------------------
void LALInferenceAdaptationRestart(LALInferenceThreadState *thread, INT4 cycle) {
    INT4 adapting=1;
    LALInferenceVariableItem *item;
    INT4 bigACL = INT_MAX;

    for (item=thread->currentParams->head; item; item=item->next) {
        if (item->vary != LALINFERENCE_PARAM_FIXED && item->vary != LALINFERENCE_PARAM_OUTPUT) {
            char tmpname[MAX_STRLEN]="";

            sprintf(tmpname, "%s_%s", item->name, ACCEPTSUFFIX);
            REAL8 *accepted = (REAL8 *)LALInferenceGetVariable(thread->proposalArgs, tmpname);
            *accepted = 0;

            sprintf(tmpname,"%s_%s",item->name,PROPOSEDSUFFIX);
            REAL8 *proposed = (REAL8 *)LALInferenceGetVariable(thread->proposalArgs, tmpname);
            *proposed = 0;
        }
    }

    INT4 length = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptLength");
    INT4 tau = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptTau");
    INT4 restartBuffer = LALInferenceGetINT4Variable(thread->proposalArgs, "adaptResetBuffer");
    REAL8 s_gamma = LALInferenceAdaptationEnvelope(cycle, cycle, length, tau, restartBuffer);
    LALInferenceAddVariable(thread->proposalArgs, "s_gamma", &s_gamma, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_OUTPUT);

    LALInferenceSetVariable(thread->proposalArgs, "adapting", &adapting);
    LALInferenceSetVariable(thread->proposalArgs, "adaptStart", &cycle);
    LALInferenceSetVariable(thread->proposalArgs, "logLAtAdaptStart", &(thread->currentLikelihood));
    LALInferenceSetVariable(thread->proposalArgs, "acl", &bigACL);
}

//-----------------------------------------
// Adaptation envelope function:
//-----------------------------------------
REAL8 LALInferenceAdaptationEnvelope(INT4 cycle, INT4 start, INT4 length, INT4 tau, INT4 reset) {
    REAL8 s_gamma = 0.0;

    if (cycle-start <= reset) {
        s_gamma=((REAL8)(cycle-start)/(REAL8)reset);
        s_gamma *= s_gamma;
    } else if (cycle-start < length) {
        s_gamma = 10.0*exp(-(1.0/tau)*log((REAL8)(cycle-start)))-1;
    } else {
        s_gamma = 0.0;
    }

    return s_gamma;
}


//-----------------------------------------
// file output routines:
//-----------------------------------------
FILE **LALInferencePrintPTMCMCHeadersOrResume(LALInferenceRunState *runState) {
    ProcessParamsTable *ppt;
    INT4 randomseed;
    INT4 MPIrank, t, n_local_threads;
    INT4 step;
    char *outFileName = NULL;
    FILE *threadoutput = NULL;
    FILE **threadoutputs = NULL;
    LALInferenceThreadState *thread;

    MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);

    n_local_threads = runState->nthreads;
    randomseed = LALInferenceGetINT4Variable(runState->algorithmParams,"random_seed");

    threadoutputs = XLALCalloc(n_local_threads, sizeof(FILE*));

    for (t = 0; t < n_local_threads; t++) {
        thread = runState->threads[t];

        ppt = LALInferenceGetProcParamVal(runState->commandLine, "--outfile");
        if (ppt) {
            outFileName = (char*)XLALCalloc(strlen(ppt->value)+255, sizeof(char*));
            if (n_local_threads*MPIrank+t == 0) {
                sprintf(outFileName,"%s",ppt->value);
                //Because of the way Pegasus handles file names, it needs exact match between --output and filename
            } else {
                sprintf(outFileName, "%s.%2.2d", ppt->value, n_local_threads*MPIrank+t);
            }

        } else {
            outFileName = (char*)XLALCalloc(255, sizeof(char*));
            sprintf(outFileName, "PTMCMC.output.%u.%2.2d", randomseed, n_local_threads*MPIrank+t);
        }

        if (LALInferenceGetProcParamVal(runState->commandLine, "--resume") && access(outFileName, R_OK) == 0) {
            /* Then file already exists for reading, and we're going to resume
            from it, so don't write the header. */

            threadoutput = fopen(outFileName, "r");
            if (threadoutput == NULL) {
                XLALErrorHandler = XLALExitErrorHandler;
                XLALPrintError("Error reading resume file (in %s, line %d)\n", __FILE__, __LINE__);
                XLAL_ERROR_NULL(XLAL_EIO);
            }

            step = LALInferenceMCMCResumeRead(thread, threadoutput);
            LALInferenceSetVariable(runState->algorithmParams, "step", &step);

            fclose(threadoutput);

            threadoutput = fopen(outFileName, "a");
        } else {
            threadoutput = fopen(outFileName,"w");
            if(threadoutput == NULL){
                XLALErrorHandler = XLALExitErrorHandler;
                XLALPrintError("Output file error. Please check that the specified path exists. (in %s, line %d)\n",__FILE__, __LINE__);
                XLAL_ERROR_NULL(XLAL_EIO);
            }

            LALInferencePrintPTMCMCHeaderFile(runState, thread, threadoutput);
        }

        if(setvbuf(threadoutput,NULL,_IOFBF,0x100000)) /* Set buffer to 1MB so as to not thrash NFS */
          fprintf(stderr,"Warning: Unable to set output file buffer!");

        threadoutputs[t] = threadoutput;

        XLALFree(outFileName);
    }

    return threadoutputs;
}

void LALInferencePrintPTMCMCHeaderFile(LALInferenceRunState *runState, LALInferenceThreadState *thread, FILE *threadoutput) {
    INT4 MPIrank, nthreads;
    INT4 randomseed, nPar, Niter;
    REAL8 timestamp;
    LALInferenceIFOData *ifodata1;
    struct timeval tv;
    char *arg_str;

    MPI_Comm_rank(MPI_COMM_WORLD, &MPIrank);

    nthreads = LALInferenceGetINT4Variable(runState->algorithmParams, "ntemp");

    randomseed = LALInferenceGetINT4Variable(runState->algorithmParams, "random_seed");
    nPar = LALInferenceGetVariableDimensionNonFixed(thread->currentParams);
    Niter = LALInferenceGetINT4Variable(runState->algorithmParams, "nsteps");

    REAL8 f_ref = 0.0;
    if(LALInferenceCheckVariable(thread->currentParams, "f_ref"))
        f_ref = LALInferenceGetREAL8Variable(thread->currentParams, "f_ref");

    UINT4 nIFO = 0;
    ifodata1 = runState->data;
    while(ifodata1){
        nIFO++;
        ifodata1 = ifodata1->next;
    }

    UINT4 waveform = 0;
    if(LALInferenceCheckVariable(thread->currentParams, "LAL_APPROXIMANT"))
        waveform = LALInferenceGetUINT4Variable(thread->currentParams, "LAL_APPROXIMANT");

    REAL8 pnorder = 0.0;
    if(LALInferenceCheckVariable(thread->currentParams,"LAL_PNORDER"))
        pnorder = ((REAL8)LALInferenceGetINT4Variable(thread->currentParams, "LAL_PNORDER"))/2.0;

    arg_str = LALInferencePrintCommandLine(runState->commandLine);

    ifodata1 = runState->data;
    REAL8 networkSNR=0.0;
    while(ifodata1){
        networkSNR += ifodata1->SNR * ifodata1->SNR;
        ifodata1 = ifodata1->next;
    }
    networkSNR = sqrt(networkSNR);

    REAL8 SampleRate = 4096.0; //default value of the sample rate from LALInferenceReadData()
    if(LALInferenceGetProcParamVal(runState->commandLine, "--srate"))
        SampleRate = atof(LALInferenceGetProcParamVal(runState->commandLine, "--srate")->value);

    UINT4 benchmark=0;
    if(LALInferenceGetProcParamVal(runState->commandLine, "--benchmark"))
        benchmark = 1;

    /* Print version info */
    fprintf(threadoutput, "  LALInference version:%s,%s,%s,%s,%s\n", lalAppsVCSId,lalAppsVCSDate,lalAppsVCSBranch,lalAppsVCSAuthor,lalAppsVCSStatus);
    fprintf(threadoutput,"  %s\n", arg_str);

    /* Print algorithm parameters */
    fprintf(threadoutput, "%10s  %6s  %20s  %6s %6s  %10s  %12s  %9s  %9s %8s %8s\n",
            "nIter", "seed", "null_likelihood", "Ndet", "nTemps",
            "Tchain", "NetworkSNR", "Waveform", "pNorder", "Npar", "f_ref");
    fprintf(threadoutput, "%10d  %u  %20.10lf  %6d %6d %12.1f %14.6f  %9i  %12.1f  %8i %12.1f\n",
            Niter, randomseed, thread->nullLikelihood, nIFO, nthreads,
            thread->temperature, networkSNR, waveform, pnorder, nPar, f_ref);

    /* Print detector-specific settings */
    fprintf(threadoutput, "\n%16s  %16s  %10s  %10s  %20s  %15s  %12s\n",
            "Detector", "SNR", "f_low", "f_high",
            "Sample_start", "Sample_length", "Sample_rate");
    ifodata1=runState->data;
    while(ifodata1){
        fprintf(threadoutput, "%16s  %16.8lf  %10.2lf  %10.2lf  %20.8lf  %15.7lf  %.1f\n",
                ifodata1->detector->frDetector.name, ifodata1->SNR, ifodata1->fLow, ifodata1->fHigh,
                XLALGPSGetREAL8(&(ifodata1->epoch)), atof(LALInferenceGetProcParamVal(runState->commandLine,"--seglen")->value),
                SampleRate);
        ifodata1=ifodata1->next;
    }

    /* Print column header */
    fprintf(threadoutput, "\n\n%31s\n","");
    fprintf(threadoutput, "cycle\tlogpost\tlogprior\t");
    LALInferenceFprintParameterNonFixedHeaders(threadoutput, thread->currentParams);

    /* Check for spline calibration parameters */
    if (LALInferenceCheckVariable(thread->currentParams, "spcal_active") &&
        (LALInferenceGetUINT4Variable(thread->currentParams, "spcal_active")))
        LALInferenceFprintSplineCalibrationHeader(threadoutput, thread);

    /* Print the likelihood and SNR of each individual detector */
    fprintf(threadoutput, "logl\t");
    fprintf(threadoutput, "deltalogl\t");
    LALInferenceIFOData *headIFO = runState->data;
    while (headIFO != NULL) {
        fprintf(threadoutput, "logl");
        fprintf(threadoutput, "%s",headIFO->name);
        fprintf(threadoutput, "\t");
        headIFO = headIFO->next;
    }
    if (LALInferenceGetProcParamVal(runState->commandLine, "--output-SNRs")) {
        headIFO = runState->data;
        while (headIFO != NULL) {
            fprintf(threadoutput, "SNR");
            fprintf(threadoutput, "%s",headIFO->name);
            fprintf(threadoutput, "\t");
            headIFO = headIFO->next;
        }
        fprintf(threadoutput, "SNR\t");
    }

    /* Print the cumulative runtime at each sample */
    if (benchmark)
        fprintf(threadoutput, "timestamp\t");
    fprintf(threadoutput,"\n");

    /* Print the starting values */
    fprintf(threadoutput, "%d\t%f\t%f\t", 0,
            (thread->currentLikelihood - thread->nullLikelihood) + thread->currentPrior,
            thread->currentPrior);

    LALInferencePrintSampleNonFixed(threadoutput, thread->currentParams);
    if (LALInferenceCheckVariable(thread->currentParams, "spcal_active") &&
        (LALInferenceGetUINT4Variable(thread->currentParams, "spcal_active")))
        LALInferencePrintSplineCalibration(threadoutput, thread);

    fprintf(threadoutput, "%f\t", thread->currentLikelihood);
    fprintf(threadoutput, "%f\t", thread->currentLikelihood - thread->nullLikelihood);
    headIFO = runState->data;
    UINT4 i = 0;
    while (headIFO != NULL) {
        fprintf(threadoutput, "%f\t", thread->currentIFOLikelihoods[i] - headIFO->nullloglikelihood);
        headIFO = headIFO->next;
        i++;
    }
    if(benchmark) {
        gettimeofday(&tv, NULL);
        timestamp = tv.tv_sec + tv.tv_usec/1E6;
        LALInferenceAddVariable(runState->algorithmParams, "timestamp_epoch", &timestamp, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
        fprintf(threadoutput, "%f\t", 0.0);
    }
    fprintf(threadoutput,"\n");
}

void LALInferencePrintPTMCMCInjectionSample(LALInferenceRunState *runState) {
    ProcessParamsTable *ppt;
    LALInferenceThreadState *thread = runState->threads[0];

    ppt = LALInferenceGetProcParamVal(runState->commandLine, "--inj");
    if (ppt) {
        ProcessParamsTable *ppt2 = LALInferenceGetProcParamVal(runState->commandLine, "--outfile");
        INT4 randomseed = LALInferenceGetINT4Variable(runState->algorithmParams, "random_seed");
        FILE *out = NULL;
        char *fname = NULL;
        LALInferenceVariables *saveParams = NULL;

        saveParams = (LALInferenceVariables *)XLALCalloc(sizeof(LALInferenceVariables), 1);

        if (ppt2) {
            fname = (char *) XLALCalloc((strlen(ppt2->value)+255)*sizeof(char), 1);
            sprintf(fname, "%s.injection", ppt2->value);
        } else {
            fname = (char *) XLALCalloc(255*sizeof(char), 1);
            sprintf(fname, "PTMCMC.output.%u.injection", randomseed);
        }
        out = fopen(fname, "w");

        LALInferenceCopyVariables(thread->currentParams, saveParams);

        SimInspiralTable *injTable = NULL;
        SimInspiralTable *theEventTable = NULL;

        SimInspiralTableFromLIGOLw(&injTable,ppt->value,0,0);

        ppt2 = LALInferenceGetProcParamVal(runState->commandLine, "--event");
        if (ppt2) {
            UINT4 event = atoi(ppt2->value);
            UINT4 i;
            theEventTable = injTable;
        for (i = 0; i < event; i++) {
            theEventTable = theEventTable->next;
        }
            theEventTable->next = NULL;
        } else {
            theEventTable=injTable;
            theEventTable->next = NULL;
        }

        REAL8 m1 = theEventTable->mass1;
        REAL8 m2 = theEventTable->mass2;
        REAL8 q = m2/m1;
        REAL8 eta = m1*m2/(m1+m2)/(m1+m2);

        if (q > 1.0) q = 1.0/q;

        REAL8 sx = theEventTable->spin1x;
        REAL8 sy = theEventTable->spin1y;
        REAL8 sz = theEventTable->spin1z;

        REAL8 a_spin1 = sqrt(sx*sx + sy*sy + sz*sz);

        REAL8 theta_spin1, phi_spin1;
        if (a_spin1 == 0.0) {
            theta_spin1 = 0.0;
            phi_spin1 = 0.0;
        } else {
            theta_spin1 = acos(sz / a_spin1);
            phi_spin1 = atan2(sy, sx);
            if (phi_spin1 < 0.0) phi_spin1 += 2.0*M_PI;
        }

        sx = theEventTable->spin2x;
        sy = theEventTable->spin2y;
        sz = theEventTable->spin2z;

        REAL8 a_spin2 = sqrt(sx*sx + sy*sy + sz*sz), theta_spin2, phi_spin2;
        if (a_spin2 == 0.0) {
            theta_spin2 = 0.0;
            phi_spin2 = 0.0;
        } else {
            theta_spin2 = acos(sz / a_spin2);
            phi_spin2 = atan2(sy, sx);
            if (phi_spin2 < 0.0) phi_spin2 += 2.0*M_PI;
        }

        REAL8 psi = theEventTable->polarization;
        if (psi>=M_PI) psi -= M_PI;

        REAL8 injGPSTime = XLALGPSGetREAL8(&(theEventTable->geocent_end_time));

        REAL8 chirpmass = theEventTable->mchirp;

        REAL8 dist = theEventTable->distance;
        REAL8 inclination = theEventTable->inclination;
        REAL8 phase = theEventTable->coa_phase;
        REAL8 dec = theEventTable->latitude;
        REAL8 ra = theEventTable->longitude;

        LALInferenceSetVariable(thread->currentParams, "chirpmass", &chirpmass);
        if (LALInferenceCheckVariable(thread->currentParams, "q")) {
            LALInferenceSetVariable(thread->currentParams, "q", &q);
        } else if (LALInferenceCheckVariable(thread->currentParams, "eta")) {
            LALInferenceSetVariable(thread->currentParams, "eta", &eta);
        } else {
            /* Restore state, cleanup, and throw error */
            LALInferenceClearVariables(thread->currentParams);
            LALInferenceCopyVariables(saveParams, thread->currentParams);
            XLALFree(fname);
            LALInferenceClearVariables(saveParams);
            XLALFree(saveParams);
            XLAL_ERROR_VOID(XLAL_EINVAL, "unknown mass ratio parameter name (allowed are 'eta' or 'q')");
        }

        UINT4 added_time_param = 0;
        if (!LALInferenceCheckVariable(thread->currentParams, "time")) {
            added_time_param = 1;
            LALInferenceAddVariable(thread->currentParams, "time", &injGPSTime, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
        } else {
            LALInferenceSetVariable(thread->currentParams, "time", &injGPSTime);
        }

        UINT4 added_phase_param = 0;
        if (!LALInferenceCheckVariable(thread->currentParams, "phase")) {
            added_phase_param = 1;
            LALInferenceAddVariable(thread->currentParams, "phase", &phase, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
        } else {
            LALInferenceSetVariable(thread->currentParams, "phase", &phase);
        }
        REAL8 cosinclination=cos(inclination);
        LALInferenceSetVariable(thread->currentParams, "distance", &dist);
        LALInferenceSetVariable(thread->currentParams, "costheta_jn", &cosinclination);
        LALInferenceSetVariable(thread->currentParams, "polarisation", &(psi));
        LALInferenceSetVariable(thread->currentParams, "declination", &dec);
        LALInferenceSetVariable(thread->currentParams, "rightascension", &ra);
        if (LALInferenceCheckVariable(thread->currentParams, "a_spin1")) {
            LALInferenceSetVariable(thread->currentParams, "a_spin1", &a_spin1);
        }
        if (LALInferenceCheckVariable(thread->currentParams, "theta_spin1")) {
            LALInferenceSetVariable(thread->currentParams, "theta_spin1", &theta_spin1);
        }
        if (LALInferenceCheckVariable(thread->currentParams, "phi_spin1")) {
            LALInferenceSetVariable(thread->currentParams, "phi_spin1", &phi_spin1);
        }
        if (LALInferenceCheckVariable(thread->currentParams, "a_spin2")) {
            LALInferenceSetVariable(thread->currentParams, "a_spin2", &a_spin2);
        }
        if (LALInferenceCheckVariable(thread->currentParams, "theta_spin2")) {
            LALInferenceSetVariable(thread->currentParams, "theta_spin2", &theta_spin2);
        }
        if (LALInferenceCheckVariable(thread->currentParams, "phi_spin2")) {
            LALInferenceSetVariable(thread->currentParams, "phi_spin2", &phi_spin2);
        }

        thread->currentLikelihood = runState->likelihood(thread->currentParams, runState->data, thread->model);
        thread->currentPrior = runState->prior(runState, thread->currentParams, thread->model);

        LALInferencePrintPTMCMCHeaderFile(runState, thread, out);

        fclose(out);

        if (added_time_param) {
            LALInferenceRemoveVariable(thread->currentParams, "time");
            LALInferenceRemoveMinMaxPrior(runState->priorArgs, "time");
        }

        if (added_phase_param) {
            LALInferenceRemoveVariable(thread->currentParams, "phase");
            LALInferenceRemoveMinMaxPrior(runState->priorArgs, "phase");
        }

        LALInferenceCopyVariables(saveParams, thread->currentParams);
        thread->currentLikelihood = runState->likelihood(thread->currentParams, runState->data, thread->model);
        thread->currentPrior = runState->prior(runState, thread->currentParams, thread->model);

        XLALFree(fname);
        LALInferenceClearVariables(saveParams);
        XLALFree(saveParams);
    }
}

void LALInferencePrintMCMCSample(LALInferenceThreadState *thread, LALInferenceIFOData *data, INT4 iteration, REAL8 timestamp, FILE *threadoutput) {
    UINT4 ifo = 0;
    LALInferenceIFOData *headIFO;

    fseek(threadoutput, 0L, SEEK_END);

    fprintf(threadoutput, "%d\t%f\t%f\t",
            iteration, (thread->currentLikelihood - thread->nullLikelihood) + thread->currentPrior, thread->currentPrior);

    LALInferencePrintSampleNonFixed(threadoutput, thread->currentParams);

    if (LALInferenceCheckVariable(thread->currentParams, "spcal_active") &&
        (LALInferenceGetUINT4Variable(thread->currentParams, "spcal_active"))) {
        LALInferencePrintSplineCalibration(threadoutput, thread);
    }

    fprintf(threadoutput,"%f\t", thread->currentLikelihood);
    fprintf(threadoutput,"%f\t", thread->currentLikelihood - thread->nullLikelihood);

    ifo = 0;
    headIFO = data;
    while (headIFO != NULL) {
        fprintf(threadoutput, "%f\t", thread->currentIFOLikelihoods[ifo] - headIFO->nullloglikelihood);
        ifo++;
        headIFO = headIFO->next;
    }

    if (LALInferenceGetINT4Variable(thread->proposalArgs, "output_snrs")) {
        headIFO = data;
        ifo = 0;
        while (headIFO != NULL) {
            fprintf(threadoutput, "%f\t", thread->currentIFOSNRs[ifo]);
            headIFO = headIFO->next;
            ifo++;
        }
        fprintf(threadoutput, "%f\t", thread->model->SNR);
    }

    if (timestamp > 0)
        fprintf(threadoutput, "%f\t", timestamp);

    fprintf(threadoutput,"\n");
    fflush(threadoutput);
    return;
}

void LALInferenceDataDump(LALInferenceIFOData *data, LALInferenceModel *model) {
    const UINT4 nameLength=256;
    char filename[nameLength];
    FILE *out;
    UINT4 ui;

    snprintf(filename, nameLength, "freqTemplatehPlus.dat");
    out = fopen(filename, "w");
    for (ui = 0; ui < model->freqhPlus->data->length; ui++) {
        REAL8 f = model->freqhPlus->deltaF * ui;
        COMPLEX16 d = model->freqhPlus->data->data[ui];

        fprintf(out, "%g %g %g\n", f, creal(d), cimag(d));
    }
    fclose(out);

    snprintf(filename, nameLength, "freqTemplatehCross.dat");
    out = fopen(filename, "w");
    for (ui = 0; ui < model->freqhCross->data->length; ui++) {
        REAL8 f = model->freqhCross->deltaF * ui;
        COMPLEX16 d = model->freqhCross->data->data[ui];

        fprintf(out, "%g %g %g\n", f, creal(d), cimag(d));
    }
    fclose(out);

    while (data != NULL) {
        snprintf(filename, nameLength, "%s-freqTemplateStrain.dat", data->name);
        out = fopen(filename, "w");
        for (ui = 0; ui < model->freqhCross->data->length; ui++) {
            REAL8 f = model->freqhCross->deltaF * ui;
            COMPLEX16 d;
            d = data->fPlus * model->freqhPlus->data->data[ui] +
            data->fCross * model->freqhCross->data->data[ui];

            fprintf(out, "%g %g %g\n", f, creal(d), cimag(d) );
        }
        fclose(out);

        snprintf(filename, nameLength, "%s-timeTemplateStrain.dat", data->name);
        out = fopen(filename, "w");
        for (ui = 0; ui < model->timehCross->data->length; ui++) {
            REAL8 tt = XLALGPSGetREAL8(&(model->timehCross->epoch)) +
            data->timeshift + ui*model->timehCross->deltaT;
            REAL8 d = data->fPlus*model->timehPlus->data->data[ui] +
            data->fCross*model->timehCross->data->data[ui];

            fprintf(out, "%.6f %g\n", tt, d);
        }
        fclose(out);

        snprintf(filename, nameLength, "%s-timeTemplatehPlus.dat", data->name);
        out = fopen(filename, "w");
        for (ui = 0; ui < model->timehPlus->data->length; ui++) {
            REAL8 tt = XLALGPSGetREAL8(&(model->timehPlus->epoch)) +
            ui * model->timehPlus->deltaT;
            REAL8 d = model->timehPlus->data->data[ui];

            fprintf(out, "%.6f %g\n", tt, d);
        }
        fclose(out);

        snprintf(filename, nameLength, "%s-timeTemplatehCross.dat", data->name);
        out = fopen(filename, "w");
        for (ui = 0; ui < model->timehCross->data->length; ui++) {
            REAL8 tt = XLALGPSGetREAL8(&(model->timehCross->epoch)) +
            ui * model->timehCross->deltaT;
            REAL8 d = model->timehCross->data->data[ui];

            fprintf(out, "%.6f %g\n", tt, d);
        }
        fclose(out);

        data = data->next;
    }
}

INT4 LALInferenceMCMCResumeRead(LALInferenceThreadState *thread, FILE *resumeFile) {
    /* Hope that the line is shorter than 16K! */
    const long len = 16384;
    char linebuf[len];
    char *last_line = NULL;
    long flen, line_length;
    int cycle;
    float loglike, logprior;

    fseek(resumeFile, 0L, SEEK_END);
    flen = ftell(resumeFile);

    if (flen < len) {
        fseek(resumeFile, 0L, SEEK_SET);
        fread(linebuf, flen, 1, resumeFile);
        linebuf[flen-1] = '\0'; /* Strip off trailing newline. */
    } else {
        fseek(resumeFile, -len, SEEK_END);
        fread(linebuf, len, 1, resumeFile);
        linebuf[len-1] = '\0'; /* Strip off trailing newline.... */
    }

    last_line = strrchr(linebuf, '\n'); /* Last occurence of '\n' */

    line_length = strlen(last_line);

    /* Go to the beginning of the last line. */
    fseek(resumeFile, -line_length, SEEK_END);

    fscanf(resumeFile, "%d %f %f", &cycle, &loglike, &logprior);

    LALInferenceReadSampleNonFixed(resumeFile, thread->currentParams);

    return cycle;
}
