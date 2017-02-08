#!/usr/bin/env python

###############################################################################
#  
#     Copyright (C) 2015 by Andrew Jameson
#     Licensed under the Academic Free License version 2.1
# 
###############################################################################

import logging
import tornado.gen
from katportalclient import KATPortalClient

import os, threading, sys, socket, select, signal, traceback
import errno, time, random

from spip.utils import sockets,times

DAEMONIZE = True
DL = 1

###############################################################
# PubSub daemon
class PubSubThread (threading.Thread):

  def __init__ (self, script, id):
    threading.Thread.__init__(self)
    self.script = script
   
    self.script.log(2, "PubSubThread.__init__()")

    self.curr_utc = times.getUTCTime()
    self.prev_utc = self.curr_utc
   
    self.metadata_server = self.script.cfg["PUBSUB_ADDRESS"]
    self.logger = logging.getLogger('katportalclient.example') 
    self.logger.setLevel(logging.INFO)
    self.beam = -1
    self.sub_array = -1
    self.subs = []
    self.io_loop = []
    self.policy = "event-rate 1.0 300.0"
    self.title  = "ptuse_unconfigured"
    self.running = False
    self.restart_io_loop = True

  def configure (self):

    self.subs = []

    dp_prefix = self.data_product_prefix.replace("_", ".")
    sa_prefix = self.sub_array_prefix.replace("_", ".")

    self.subs.append ( dp_prefix + ".cbf.synchronisation.epoch")
    self.subs.append ( sa_prefix +".state")
    self.subs.append ( sa_prefix +".pool.resources")
    self.subs.append ( sa_prefix +".script.target")
    self.subs.append ( sa_prefix +".script.ra")
    self.subs.append ( sa_prefix +".script.dec")
    self.subs.append ( sa_prefix +".script.ants")
    self.subs.append ( sa_prefix +".script.observer")
    self.subs.append ( sa_prefix +".script.tsubint")
    self.subs.append ( sa_prefix +".script.experiment.id")
    self.subs.append ( sa_prefix +".script.active.sbs")
    self.subs.append ( sa_prefix +".script.description")

  # configure the pub/sub instance to 
  def set_sub_array (self, sub_array, beam):
    self.script.log(2, "PubSubThread::set_sub_array sub_array="+ str(sub_array) + " beam=" + str(beam))
    self.sub_array = str(sub_array)
    self.beam = str(beam )
    self.data_product_prefix = "data_" + self.sub_array
    self.sub_array_prefix = "subarray_" + self.sub_array
    self.title = "ptuse_beam_" + str(beam)
    self.configure()

  def run (self):
    self.script.log(1, "PubSubThread::run starting while")
    while self.restart_io_loop:

      # open connection to CAM
      self.io_loop = tornado.ioloop.IOLoop.current()
      self.io_loop.add_callback (self.connect, self.logger)

      self.running = True
      self.restart_io_loop = False
      self.io_loop.start()
      self.running = False
      self.io_loop = []

      # unsubscribe and disconnect from CAM
      self.ws_client.unsubscribe(self.title)
      self.ws_client.disconnect()

    self.script.log(2, "PubSubThread::run exiting")

  def join (self):
    self.script.log(2, "PubSubThread::join self.stop()")
    self.stop()

  def stop (self):
    self.script.log(2, "PubSubThread::stop()")
    if self.running:
      self.script.log(2, "PubSubThread::stop io_loop.stop()")
      self.io_loop.stop()
    return

  def restart (self):
    # get the IO loop to restart on the call to stop()
    self.restart_io_loop = True
    if self.running:
      self.script.log(2, "PubSubThread::restart self.stop()")
      self.stop()
    return

  @tornado.gen.coroutine
  def connect (self, logger):
    self.script.log(2, "PubSubThread::connect()")
    self.ws_client = KATPortalClient(self.metadata_server, self.on_update_callback, logger=logger)
    self.script.log(2, "PubSubThread::connect self.ws_client.connect()")
    yield self.ws_client.connect()
    self.script.log(2, "PubSubThread::connect self.ws_client.subscribe(" + self.title + ")")
    result = yield self.ws_client.subscribe(self.title)
    self.script.log(2, "PubSubThread::connect self.ws_client.set_sampling_strategies (" + self.title + ", " + str(self.subs) + ", " + self.policy + ")")
    results = yield self.ws_client.set_sampling_strategies( self.title, self.subs, self.policy) 

    for result in results:
      self.script.log(2, "PubSubThread::connect subscribed to " + str(result))

  def on_update_callback (self, msg):

    self.curr_utc = times.getUTCTime()
    if times.diffUTCTimes(self.prev_utc, self.curr_utc) > 60:
      self.script.log(2, "PubSubThread::on_update_callback: heartbeat msg="+str(msg))
      self.prev_utc = self.curr_utc

    self.update_config (msg)

  def update_cam_config (self, key, name, value):
    if self.script.cam_config[key] != value:
      self.script.log(1, "PubSubThread::update_cam_config " + key + "=" + value + " from " + name)
      self.script.cam_config[key] = value

  def update_config (self, msg):

    # ignore empty messages
    if msg == []: 
      return

    status = msg["msg_data"]["status"]
    value = msg["msg_data"]["value"]
    name = msg["msg_data"]["name"]

    self.script.log(2, "PubSubThread::update_config " + name + "=" + str(value))
   
    if name == self.sub_array_prefix + "_script_target":
      self.update_cam_config("SOURCE", name, str(value))

    elif name == self.sub_array_prefix + "_script_ra":
      self.update_cam_config("RA", name, str(value))

    elif name == self.sub_array_prefix + "_script_dec":
      self.update_cam_config("DEC", name, str(value))

    elif name == self.data_product_prefix + "_cbf_synchronisation_epoch":
      self.update_cam_config("ADC_SYNC_TIME", name, str(value))

    elif name == self.sub_array_prefix + "_script_observer":
      self.update_cam_config("OBSERVER", name, str(value))

    elif name == self.sub_array_prefix + "_script_tsubint":
      self.update_cam_config("TSUBINT", name, str(value))

    elif name == self.sub_array_prefix + "_script_ants":
      self.update_cam_config("ANTENNAE", name, str(value))

    elif name == self.sub_array_prefix + "_active_sbs":
      self.update_cam_config("SCHEDULE_BLOCK_ID", name, str(value))

    elif name == self.sub_array_prefix + "_script_experiment_id":
      self.update_cam_config("EXPERIMENT_ID", name, str(value))

    elif name == self.sub_array_prefix + "_pool_resources":
      self.update_cam_config("POOL_RESOURCES", name, str(value))

    elif name == self.sub_array_prefix + "_script_description":
      self.update_cam_config("DESCRIPTION", name, str(value))

    elif name == self.sub_array_prefix + "_state":
      self.update_cam_config("SUBARRAY_STATE", name, str(value))

    else:
      self.script.log(1, "PubSubThread::update_config no match on " + name)

    self.script.log(3, "PubSubThread::update_config done")

