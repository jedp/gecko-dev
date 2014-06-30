/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://gre/modules/Services.jsm");

/* jshint unused:false */
function run_test() {
  run_next_test();
}

add_task(function test_smoke() {
  do_check_eq("function", typeof MockWebSocketChannel);
  do_test_finished();
});

// XXX probably want to make these helper methods in head_channel

add_task(function test_helo() {
  let channel = new MockWebSocketChannel();

  do_check_eq("function", typeof channel.close);

  let callback = function(type, params) {
    switch (type) {
      case "onStart":
        channel.sendMsg("HELO:v1:1800");
        break;
      case "onMessageAvailable":
        do_check_eq(params.message, "HELO:v1");
        do_test_finished();
        break;
    }
  };

  channel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(callback),
    null
  );
});

add_task(function test_helo_auth_new() {
  let channel = new MockWebSocketChannel();

  let callback = function(type, params) {
    switch (type) {
      // First we say HELO ...
      case "onStart":
        channel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we ask for an id and key
        if (params.message == "HELO:v1") {
          channel.sendMsg("AUTH:");
        }

        // So we should get an id and a key
        else if (params.message == "AUTH:NEW:first-id:first-key") {
          do_test_finished();
        }

        // And there should be no other messages
        else {
          do_throw(new Error("got weird response: " + params.message));
        }

        break;
    }
  };

  channel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(callback),
    null
  );
});

add_task(function test_helo_reauth_success() {
  let expected = "AUTH:SUCCESS";
  let channel = new MockWebSocketChannel(
    {"AUTH:first-id:first-key": expected});

  let callback = function(type, params) {
    switch (type) {
      // First we say HELO ...
      case "onStart":
        channel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we auth with existing id and key
        if (params.message == "HELO:v1") {
          channel.sendMsg("AUTH:first-id:first-key");
        }

        // And that succeeds
        else if (params.message == expected) {
          do_test_finished();
        }

        // And there should be no other messages
        else {
          do_throw(new Error("got weird response: " + params.message));
        }

        break;
    }
  };

  channel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(callback),
    null
  );
});

add_task(function test_helo_reauth_retry_then_success() {
  let retried = false;
  let channel = new MockWebSocketChannel(
    {"AUTH:first-id:first-key": "AUTH:RETRY"});

  let callback = function(type, params) {
    switch (type) {
      // First we say HELO ...
      case "onStart":
        channel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we auth with existing id and key
        if (params.message == "HELO:v1") {
          channel.sendMsg("AUTH:first-id:first-key");
        }

        // Try again, expecting success
        else if (params.message == "AUTH:RETRY") {
          retried = true;
          channel.routes["AUTH:first-id:first-key"] = "AUTH:SUCCESS";
          channel.sendMsg("AUTH:first-id:first-key");
        }

        else if (params.message == "AUTH:SUCCESS") {
          do_check_true(retried);
          do_test_finished();
        }

        // And there should be no other messages
        else {
          do_throw(new Error("got weird response: " + params.message));
        }

        break;
    }
  };

  channel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(callback),
    null
  );
});

add_task(function test_helo_reauth_invalid() {
  let expected = "AUTH:INVALID";
  let channel = new MockWebSocketChannel(
    {"AUTH:first-id:first-key": expected});

  let callback = function(type, params) {
    switch (type) {
      // First we say HELO ...
      case "onStart":
        channel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we auth with existing id and key
        if (params.message == "HELO:v1") {
          channel.sendMsg("AUTH:first-id:first-key");
        }

        // And for some reason it's invalid
        else if (params.message == expected) {
          do_test_finished();
        }

        // And there should be no other messages
        else {
          do_throw(new Error("got weird response: " + params.message));
        }

        break;
    }
  };

  channel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(callback),
    null
  );
});

add_task(function test_helo_reauth_redirect() {
  let redirect = "AUTH:REDIRECT:cluster42.svcs.example.org";
  let firstChannel = new MockWebSocketChannel(
    {"AUTH:first-id:first-key": redirect});

  let secondChannel;

  // Callback for the second channel, after we've been redirected
  let secondCallback = function(type, params) {
    switch (type) {
      case "onStart":
        secondChannel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we auth with existing id and key
        if (params.message == "HELO:v1") {
          secondChannel.sendMsg("AUTH:first-id:first-key");
        }

        else if (params.message == "AUTH:SUCCESS") {
          do_test_finished();
        }

        else {
          do_throw("Expected HELO:v1; got " + params.message);
        }
        break;
    }
  };

  // Callback for the first channel, which will redirect us
  let firstCallback = function(type, params) {
    switch (type) {
      case "onStart":
        firstChannel.sendMsg("HELO:v1:1800");
        break;

      case "onMessageAvailable":
        // After we've said HELO, we auth with existing id and key
        if (params.message == "HELO:v1") {
          firstChannel.sendMsg("AUTH:first-id:first-key");
        }

        else if (params.message == redirect) {
          secondChannel = new MockWebSocketChannel(
            {"AUTH:first-id:first-key": "AUTH:SUCCESS"});
          let origin = "ws://" + params.message.split(":")[2];
          let uri = Services.io.newURI(origin + "/", null, null);
          secondChannel.asyncOpen(
            uri,
            origin,
            new MockWebSocketListener(secondCallback),
            null
          );
        }

        else {
          do_throw("Expected redirect; got " + params.message);
        }
        break;
    }
  };

  firstChannel.asyncOpen(
    testURI,
    testOrigin,
    new MockWebSocketListener(firstCallback),
    null
  );
});
