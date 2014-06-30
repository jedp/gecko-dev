/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const testOrigin = "ws://example.org";
const testURI = Services.io.newURI(testOrigin + "/", null, null);

/* jshint unused:false */
function MockWebSocketChannel(routes = {}) {
  this.listener = null;
  this.context = null;

  // Assign default routes or their overrides
  this.routes = {};
  for (let [key,value] of [
    ["HELO:v1:1800",            "HELO:v1"],
    ["AUTH:",                   "AUTH:NEW:first-id:first-key"],
    ["AUTH:first-id:first-key", "AUTH:SUCCESS"],
    ["PING",                    "PONG"],
  ]) {
    this.routes[key] = routes[key] || value;
  }

  this.asyncOpen = (uri, origin, listener, context) => {
    this.uri = uri;
    this.origin = origin;
    this.listener = listener;
    this.context = context;
    this.listener.onStart(this.context);
  };

  this.close = (code) => {
    this.listener.onStop(this.context, code);
  };

  this.sendMsg = (message) => {
    dump("Route " + message + " -> " + this.routes[message] + "\n");
    this.listener.onMessageAvailable(this.context, this.routes[message]);
  };
}

// XXX replace me with the Channel.jsm listener
/* jshint unused:false */
function MockWebSocketListener(callback) {
  this.callback = callback;

  this.onStart = (context) => {
    this.callback("onStart", {context: context});
  };

  this.onStop = (context, statusCode) => {
    this.callback("onStop", {context: context, suatusCode: statusCode});
  };

  this.onMessageAvailable = (context, message) => {
    this.callback("onMessageAvailable", {context: context, message: message});
  };

  this.onAcknowledge = (context, size) => {
    this.callback("onStop", {context: context, size: size});
  };
}
