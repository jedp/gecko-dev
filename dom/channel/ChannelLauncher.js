/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function ChannelServiceLauncher() {}

ChannelServiceLauncher.prototype = {
  constructor: ChannelServiceLauncher,
  classID: Components.ID("{8b14fef2-d8df-416f-8a88-60c7f7f37c83}"),
  contractID: "@mozilla.org/channel/ServiceLauncher;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        Services.obs.addObserver(this, "final-ui-startup", true);
        break;
      case "final-ui-startup":
        Services.obs.removeObserver(this, "final-ui-startup");
        Cu.import("resource://gre/modules/Channel.jsm");
        ChannelService.init();
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ChannelServiceLauncher]);
