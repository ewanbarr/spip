###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from ReportingThread import ReportingThread

class TCSReportingThread(ReportingThread):

  def __init__ (self, script):
    self.script.log (1, "TCSReportingThread::TCSReportingThread()")
    self.script = script
    host = spip.getHostNameShort()
    port = self.script.cfg["TCS_REPORT_PORT"]
    ReportingThread.__init__(self, script, host, port)

  def run (self):
    self.script.log (1, "TCSReportingThread::run")
    ReportingThread.run(self)

  def parse_message(xml):
    self.script.log (1, "TCSReportingThread::parse_message")
    return "OK - TCS"


