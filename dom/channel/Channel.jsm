/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

dump("*** CHANNEL ***: Load Channel.jsm rev 1\n");

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// https://developer.mozilla.org/en-US/Add-ons/Code_snippets/Preferences
const prefs = new Preferences("services.channel.");

// https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Using
this.EXPORTED_SYMBOLS = ["ChannelService"];

function getNetworkStateChangeEventName() {
  try {
    // Firefox OS.
    Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
    return "network-active-changed";
  } catch (exception) {
    // Desktop.
    return "network:offline-status-changed";
  }
}

// https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIWebSocketListener
function SocketListener() {
  // ...
}

SocketListener.prototype = {
  constructor: SocketListener,
  onAcknowledge: function onAcknowledge(context, bufferedAmount) {
    // ...
  },
  onBinaryMessageAvailable: function onBinaryMessageAvailable(context, message) {
    // ...
  },
  onMessageAvailable: function onMessageAvailable(context, message) {
    dump("*** CHANNEL ***: Received message: " + message + "\n");
  },
  onServerClose: function onServerClose(context, statusCode, reason) {
    dump("*** CHANNEL ***: Server closed WebSocket stream closed with status=" + statusCode + " reason=" + reason + ".\n");
  },
  onStart: function onStart(context) {
    dump("*** CHANNEL ***: WebSocket stream opened.\n");
  },
  onStop: function onStop(context, statusCode) {
    dump("*** CHANNEL ***: WebSocket stream closed with status " + statusCode + ".\n");
  }
};

this.ChannelService = {
  socket: null,
  messageManager: null,
  networkStateChangeEventName: "",

  observers: [
    "xpcom-shutdown",
    "webapps-clear-data",
  ],

  messages: [
    "Channel:Register",
    "Channel:Unregister"
  ],

  prefs: [
    "serverURL"
  ],

  closeSocket: function closeSocket() {
    try {
      this.socket.close(0, null);
    } catch (exception) {
      dump("*** CHANNEL ***: Ignoring socket close error: " + exception + "\n");
    }
    this.socket = null;
  },

  createSocket: function createSocket() {
    if (this.socket) {
      this.closeSocket();
    }

    if (Services.io.offline) {
      debug("Network is offline.");
      return;
    }

    let serverURL = prefs.get("serverURL");
    if (!serverURL) {
      dump("*** CHANNEL ***: Missing serverURL\n");
      return;
    }

    let uri;
    try {
      uri = Services.io.newURI(serverURL, null, null);
    } catch (exception) {
      dump("*** CHANNEL ***: Malformed serverURL: " + serverURL + " Error: " + exception + "\n");
      return;
    }

    if (uri.scheme != "ws") {
      // TODO: Disable `ws`; enable `wss` for production.
      dump("*** CHANNEL ***: serverURL scheme must be ws://.\n");
      return;
    }

    dump("*** CHANNEL ***: Creating socket.\n");

    // https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIWebSocketChannel
    let socket = Cc["@mozilla.org/network/protocol;1?name=ws"]
                   .createInstance(Ci.nsIWebSocketChannel);

    this.socket = socket;
    let listener = new SocketListener();
    socket.asyncOpen(uri, serverURL, listener, null);
  },

  init: function init() {
    dump("*** CHANNEL ***: Initialize Channel.jsm\n");

    let messageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                           .getService(Ci.nsIMessageBroadcaster);

    if (!messageManager) {
      return;
    }

    this.messageManager = messageManager;
    this.networkStateChangeEventName = getNetworkStateChangeEventName();

    for (let message of this.messages) {
      this.messageManager.addMessageListener(message, this);
    }

    for (let observer of this.observers) {
      Services.obs.addObserver(this, observer, false);
    }
    Services.obs.addObserver(this, this.networkStateChangeEventName, false);

    for (let pref of this.prefs) {
      prefs.observe(pref, this);
    }

    this.createSocket();
    dump("*** CHANNEL ***: Channel.jsm initialized\n");
  },

  uninit: function uninit() {
    if (!this.messageManager) {
      return;
    }
    for (let message of this.messages) {
      this.messageManager.removeMessageListener(message, this);
    }
    for (let observer of this.observers) {
      Services.obs.removeObserver(this, observer, false);
    }
    Services.obs.removeObserver(this, this.networkStateChangeEventName, false);
    for (let pref of this.prefs) {
      prefs.ignore(pref, this);
    }
  },

  observe: function observe(subject, topic, data) {
    // https://developer.mozilla.org/en-US/docs/Observer_Notifications
    if (topic == "xpcom-shutdown") {
      this.uninit();
    }
    switch (topic) {
      // https://wiki.mozilla.org/XPCOM_Shutdown
      case "xpcom-shutdown":
      case "network-active-changed":
      case "network:offline-status-changed":
        // ...
        if (topic == "network-active-changed" || data == "online") {
          // ...
        }
        break;
      case "nsPref:changed":
        dump("*** CHANNEL ***: Pref changed: " + data + "\n");
        if (data != "services.channel.serverURL") {
          break;
        }
        this.createSocket();
        break;
      case "webapps-clear-data":
        let appData = subject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!appData) {
          return;
        }
        if (appData.browserOnly) {
          return;
        }
        break;
    }
  },

  receiveMessage: function receiveMessage(data) {
    let message = data.json;
    dump("*** CHANNEL ***: Message received: " + JSON.stringify(message) + "\n");
    switch (data.name) {
      case "Channel:Register":
        // ...
        break;
      case "Channel:Unregister":
        // ...
        break;
    }
  }
};
