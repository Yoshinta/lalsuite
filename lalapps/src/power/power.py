"""
Classes needed for the excess power analysis pipeline.
"""

__author__ = 'Duncan Brown <duncan@gravity.phys.uwm.edu>'
__date__ = '$Date$'
__version__ = '$Revision$'[11:-2]

import string
import exceptions
from glue import pipeline


class PowerError(exceptions.Exception):
  def __init__(self, args=None):
    self.args = args


class DataFindJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
  """
  A LSCdataFind job used by the power pipeline. The static options are
  read from the section [datafind] in the ini file. The stdout from
  LSCdataFind contains the paths to the frame files and is directed to a file
  in the cache directory named by site and GPS start and end times. The stderr
  is directed to the logs directory. The job always runs in the scheduler
  universe. The path to the executable is determined from the ini file.
  """
  def __init__(self,cp):
    """
    cp = ConfigParser object from which options are read.
    """
    self.__executable = cp.get('condor','datafind')
    self.__universe = 'scheduler'
    pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
    pipeline.AnalysisJob.__init__(self,cp)

    for sec in ['datafind']:
      self.add_ini_opts(cp,sec)

    self.add_condor_cmd('environment',
      """LD_LIBRARY_PATH=$ENV(LD_LIBRARY_PATH);PYTHONPATH=$ENV(PYTHONPATH)""" )

    self.set_stderr_file('logs/datafind-$(macroinstrument)-$(macrostart)-$(macroend)-$(cluster)-$(process).err')
    self.set_stdout_file('cache/$(macroinstrument)-$(macrostart)-$(macroend).cache')
    self.set_sub_file('datafind.sub')


class MdcDataFindJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
  """
  A MdcdataFind job used by the power pipeline. The static options are
  read from the section [mdcdatafind] in the ini file. The stdout from
  MdcdataFind contains the paths to the frame files and is directed to a file
  in the cache directory named by site and GPS start and end times. The stderr
  is directed to the logs directory. The job always runs in the scheduler
  universe. The path to the executable is determined from the ini file.
  """
  def __init__(self,cp):
    """
    cp = ConfigParser object from which options are read.
    """
    self.__executable = cp.get('condor','datafind')
    self.__universe = 'scheduler'
    pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
    pipeline.AnalysisJob.__init__(self,cp)

    for sec in ['mdcdatafind']:
      self.add_ini_opts(cp,sec)

    self.add_condor_cmd('environment',
      """LD_LIBRARY_PATH=$ENV(LD_LIBRARY_PATH);PYTHONPATH=$ENV(PYTHONPATH)""" )

    self.set_stderr_file('logs/mdcdatafind-$(macroinstrument)-$(macrostart)-$(macroend)-$(cluster)-$(process).err')
    self.set_stdout_file('mdccache/$(macroinstrument)-$(macrostart)-$(macroend).cache')
    self.set_sub_file('mdcdatafind.sub')


class InjJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
  """
  A lalapps_binj job used by the power pipeline. The static options
  are read from the sections [data] and [power] in the ini file. The
  stdout and stderr from the job are directed to the logs directory. The job
  runs in the universe specified in the ini file. The path to the executable
  is determined from the ini file.
  """
  def __init__(self,cp):
    """
    cp = ConfigParser object from which options are read.
    """
    self.__executable = cp.get('condor','injection')
    self.__universe = cp.get('condor','universe')
    pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
    pipeline.AnalysisJob.__init__(self,cp)

    for sec in ['injection']:
      self.add_ini_opts(cp,sec)

    self.set_stdout_file('logs/inj-$(macrochannelname)-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).out')
    self.set_stderr_file('logs/inj-$(macrochannelname)-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).err')
    self.set_sub_file('injection.sub')


class PowerJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
  """
  A lalapps_power job used by the power pipeline. The static options
  are read from the sections [data] and [power] in the ini file. The
  stdout and stderr from the job are directed to the logs directory. The job
  runs in the universe specified in the ini file. The path to the executable
  is determined from the ini file.
  """
  def __init__(self,cp):
    """
    cp = ConfigParser object from which options are read.
    """
    self.__executable = cp.get('condor','power')
    self.__universe = cp.get('condor','universe')
    pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
    pipeline.AnalysisJob.__init__(self,cp)

    for sec in ['data','power']:
      self.add_ini_opts(cp,sec)

    self.set_stdout_file('logs/power-$(macrochannelname)-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).out')
    self.set_stderr_file('logs/power-$(macrochannelname)-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).err')
    self.set_sub_file('power.sub')

    
class BurcaJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
  """
  A lalapps_burca job used by the power pipeline. The static options are
  read from the section [burca] in the ini file.  The stdout and stderr from
  the job are directed to the logs directory.  The path to the executable is 
  determined from the ini file.
  """
  def __init__(self,cp):
    """
    cp = ConfigParser object from which options are read.
    """
    self.__executable = cp.get('condor','burca')
    self.__universe = cp.get('condor','universe')
    pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
    pipeline.AnalysisJob.__init__(self,cp)
    
    for sec in ['burca']:
      self.add_ini_opts(cp,sec)

    self.set_stdout_file('logs/burca-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).out')
    self.set_stderr_file('logs/burca-$(macrogpsstarttime)-$(macrogpsendtime)-$(cluster)-$(process).err')
    self.set_sub_file('burca.sub')


class DataFindNode(pipeline.CondorDAGNode, pipeline.AnalysisNode):
  """
  A DataFindNode runs an instance of datafind in a Condor DAG.
  """
  def __init__(self,job):
    """
    job = A CondorDAGJob that can run an instance of LSCdataFind.
    """
    pipeline.CondorDAGNode.__init__(self,job)
    pipeline.AnalysisNode.__init__(self)
    self.__start = 0
    self.__end = 0
    self.__instrument = None
    self.__output = None
   
  def __set_output(self):
    """
    Private method to set the file to write the cache to. Automaticaly set
    once the ifo, start and end times have been set.
    """
    if self.__start and self.__end and self.__instrument:
      self.__output = 'cache/' + self.__instrument + '-' + str(self.__start) 
      self.__output = self.__output + '-' + str(self.__end) + '.cache'

  def set_start(self,time):
    """
    Set the start time of the datafind query.
    time = GPS start time of query.
    """
    self.add_var_opt('start', time)
    self.__start = time
    self.__set_output()

  def set_end(self,time):
    """
    Set the end time of the datafind query.
    time = GPS end time of query.
    """
    self.add_var_opt('end', time)
    self.__end = time
    self.__set_output()

  def set_ifo(self,ifo):
    """
    Set the IFO to retrieve data for. Since the data from both Hanford 
    interferometers is stored in the same frame file, this takes the first 
    letter of the IFO (e.g. L or H) and passes it to the --instrument option
    of LSCdataFind.
    ifo = IFO to obtain data for.
    """
    self.add_var_opt('instrument',ifo[0])
    self.__instrument = ifo[0]
    self.__set_output()

  def get_output(self):
    """
    Return the output file, i.e. the file containing the frame cache data.
    """
    return self.__output

class MdcDataFindNode(pipeline.CondorDAGNode, pipeline.AnalysisNode):
  """
  A DataFindNode runs an instance of datafind in a Condor DAG.
  """
  def __init__(self,job):
    """
    job = A CondorDAGJob that can run an instance of LSCdataFind.
    """
    pipeline.CondorDAGNode.__init__(self,job)
    pipeline.AnalysisNode.__init__(self)
    self.__start = 0
    self.__end = 0
    self.__instrument = None
    self.__output = None
   
  def __set_output(self):
    """
    Private method to set the file to write the cache to. Automaticaly set
    once the ifo, start and end times have been set.
    """
    if self.__start and self.__end and self.__instrument:
      self.__output = 'cache/' + self.__instrument + '-' + str(self.__start) 
      self.__output = self.__output + '-' + str(self.__end) + '.cache'

  def set_start(self,time):
    """
    Set the start time of the datafind query.
    time = GPS start time of query.
    """
    self.add_var_opt('start', time)
    self.__start = time
    self.__set_output()

  def set_end(self,time):
    """
    Set the end time of the datafind query.
    time = GPS end time of query.
    """
    self.add_var_opt('end', time)
    self.__end = time
    self.__set_output()

  def set_ifo(self,ifo):
    """
    Set the IFO to retrieve data for. Since the data from both Hanford 
    interferometers is stored in the same frame file, this takes the first 
    letter of the IFO (e.g. L or H) and passes it to the --instrument option
    of LSCdataFind.
    ifo = IFO to obtain data for.
    """
    self.add_var_opt('instrument',ifo[0])
    self.__instrument = ifo[0]
    self.__set_output()

  def get_output(self):
    """
    Return the output file, i.e. the file containing the frame cache data.
    """
    return self.__output

  def set_observatory(self,obs):
    """
    Set the IFO to retrieve data for. Since the data from both Hanford 
    interferometers is stored in the same frame file, this takes the first 
    letter of the IFO (e.g. L or H) and passes it to the --observatory option
    of LSCdataFind.
    @param obs: IFO to obtain data for.
    """
    self.add_var_opt('observatory',obs)
    self.__observatory = obs
    self.__set_output()
  

class InjNode(pipeline.CondorDAGNode, pipeline.AnalysisNode):
  """
  A InjNode runs an instance of binj code
  """
  def __init__(self,job):
    """
    job = A CondorDAGJob that can run an instance of lalapps_power.
    """
    pipeline.CondorDAGNode.__init__(self,job)
    pipeline.AnalysisNode.__init__(self)
    self.__usertag = job.get_config('pipeline','user-tag')
    
  def get_output(self):
    """
    Returns the file name of output from the power code. This must be kept
    synchronized with the name of the output file in power.c.
    """
    if not self.get_start() or not self.get_end():
      raise InjError, "Start time or end time has not been set"

    basename = 'HL' + '-INJECTIONS_1'

    if self.__usertag:
      basename += '_' + self.__usertag

    return basename + '-' + str(self.get_start()) + '-' + \
      str(self.get_end() - self.get_start()) + '.xml'

  
class PowerNode(pipeline.CondorDAGNode, pipeline.AnalysisNode):
  """
  A PowerNode runs an instance of the power code in a Condor DAG.
  """
  def __init__(self,job):
    """
    job = A CondorDAGJob that can run an instance of lalapps_power.
    """
    pipeline.CondorDAGNode.__init__(self,job)
    pipeline.AnalysisNode.__init__(self)
    self.__usertag = job.get_config('pipeline','user-tag')

  def get_output(self):
    """
    Returns the file name of output from the power code. This must be kept
    synchronized with the name of the output file in power.c.
    """
    if not self.get_start() or not self.get_end() or not self.get_ifo():
      raise PowerError, "Start time, end time or ifo has not been set"

    basename = self.get_ifo() + '--POWER'

    if self.get_ifo_tag():
      basename += '_' + self.get_ifo_tag()
    if self.__usertag:
      basename += '_' + self.__usertag

    return basename + '-' + str(self.get_start()) + '-' + \
      str(self.get_end() - self.get_start()) + '.xml'

  def set_mdccache(self,file):
    """
    Set the LAL frame cache to to use. The frame cache is passed to the job
    with the --frame-cache argument.
    @param file: calibration file to use.
    """
    self.add_var_opt('mdcframe-cache', file)
    self.add_input_file(file)

  def set_inj(self,file):
    """
    Set the LAL frame cache to to use. The frame cache is passed to the job
    with the --frame-cache argument.
    @param file: calibration file to use.
    """
    self.add_var_opt('injection-file', file)
    self.add_input_file(file)

class BurcaNode(pipeline.CondorDAGNode, pipeline.AnalysisNode):
  """
  A BurcaNode runs an instance of the burca code in a Condor DAG.
  """
  def __init__(self,job):
    """
    job = A CondorDAGJob that can run an instance of lalapps_burca.
    """
    pipeline.CondorDAGNode.__init__(self,job)
    pipeline.AnalysisNode.__init__(self)
    self.__usertag = job.get_config('pipeline','user-tag')
    self.__start = 0
    self.__end = 0
    self.__output = None

  def __set_output(self):
    """
    Private method to set the file to write the cache to. Automaticaly set
    once the ifo, start and end times have been set.
    """
    if self.__start and self.__end :
      self.__output = 'coin/' + 'HL' + '-' + str(self.__start) 
      self.__output = self.__output + '-' + str(self.__end) + '.xml'
                     
  def set_ifoa(self,file):
    """
    Set the LAL frame cache to to use. The frame cache is passed to the job
    with the --frame-cache argument.
    @param file: calibration file to use.
    """
    self.add_var_opt('ifo-a', file)
    self.add_input_file(file)

  def set_ifob(self,file):
    """
    Set the LAL frame cache to to use. The frame cache is passed to the job
    with the --frame-cache argument.
    @param file: calibration file to use.
    """
    self.add_var_opt('ifo-b', file)
    self.add_input_file(file)   

  def set_start(self,time):
    """
    Set the start time of the datafind query.
    time = GPS start time of query.
    """
    self.__start = time
    self.__set_output()

  def set_end(self,time):
    """
    Set the end time of the datafind query.
    time = GPS end time of query.
    """
    self.__end = time
    self.__set_output()

  def get_output(self):
    """
    Return the output file, i.e. the file containing the frame cache data.
    """
    return self.__output
