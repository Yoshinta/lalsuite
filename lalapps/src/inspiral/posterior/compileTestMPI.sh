#openmpicc -Wall -ggdb -std=gnu99 -o lalinference_mcmcmpi LALInference.c LALInferenceMainMPI.c LALInferenceTemplate.c LALInferenceReadData.c LALInferenceMCMCMPI.c `pkg-config --cflags --libs lal libframe lalsupport libmetaio lalmetaio lalframe fftw3 lalinspiral lalpulsar`
openmpicc -Wall -ggdb -std=gnu99 -o lalinference_mcmcmpi LALInference.c LALInferenceMainMPI.c LALInferenceTemplate.c LALInferenceReadData.c LALInferenceMCMCMPI.c `pkg-config --cflags --libs lal libframe lalsupport libmetaio lalmetaio lalframe fftw3 lalinspiral lalpulsar lalstochastic lalburst` -I/Users/vivien//src/lalsuite/lalapps/src/lalapps -I/Users/vivien/src/lalsuite/lalapps/src -L/Users/vivien//src/lalsuite/lalapps/src/lalapps/.libs -llalapps
