#!/usr/bin/env python
from __future__ import generators
import user
import config.base
import os

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.arch         = self.framework.require('PETSc.utilities.arch', self)
    return

  def __str__(self):
    return ''
    
  def setupHelp(self, help):
    import nargs
    help.addArgument('PETSc', '-with-etags=<bool>',            nargs.ArgBool(None, 1, 'Build etags if they do not exist'))
    return

  def configureETags(self):
    '''Determine if etags files exist and try to create otherwise'''
    if not os.path.exists(os.path.join(self.arch.dir, 'TAGS')):
      self.framework.log.write('WARNING: ETags files have not been created\n')
      self.framework.getExecutable('etags', getFullPath = 1)
      if hasattr(self.framework, 'etags'):
        pd = self.arch.dir
        if pd[-1]=='/': pd = pd[:-1] # etags chokes if there's a trailing /
        self.framework.log.write('           Running '+self.framework.etags+' to generate TAGS files\n')
        try:
          (output, error, status) = config.base.Configure.executeShellCommand('maint/generateetags.py', timeout = 15*60.0, log = self.framework.log)
          self.framework.actions.addArgument('PETSc', 'File creation', 'Generated etags files in '+pd)
        except RuntimeError, e:
          self.framework.log.write('*******Error generating etags files: '+str(e)+'*******\n')
      else:
        self.framework.log.write('           The etags command is not in your path, cannot build etags files\n')
    else:
      self.framework.log.write('Found etags file \n')
    return

  def configure(self):
    if self.framework.argDB['with-etags']:                                    
      self.executeTest(self.configureETags)
    return
