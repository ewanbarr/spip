###############################################################################
#  
#     Copyright (C) 2016 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

from spip.utils.core import system

# test whether the specified target exists in the pulsar catalog
def test_pulsar_valid (target):

  (reply, message) = get_psrcat_param (target, "name")
  if reply != "ok":
    return (reply, message)

  if message == target:
    return ("ok", "")
  else:
    return ("fail", "pulsar " + target + " did not exist in catalog")

def get_psrcat_param (target, param):
  cmd = "psrcat -all " + target + " -c " + param + " -nohead -o short"
  rval, lines = system (cmd)
  if rval != 0 or len(lines) <= 0:
    return ("fail", "could not use psrcat")

  if lines[0].startswith("WARNING"):
    return ("fail", "pulsar " + target + " did not exist in catalog " + lines[0])

  parts = lines[0].split()
  if len(parts) == 2 and parts[0] == "1":
    return ("ok", parts[1])
