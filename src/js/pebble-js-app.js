var config_url = "http://mtc.nfshost.com/sunset-watch-config.html";

Pebble.addEventListener("ready",
  function(e) {
    console.log("JS starting...");
    
    sendTimezoneToWatch(); 
    
    navigator.geolocation.getCurrentPosition(
      locationSuccess,
      locationError,
      {
          timeout: 15000, 
          maximumAge: Infinity 
          /*, enableHighAccuracy:false*/
      }
    );
  }
);

function locationError(err) {
  console.log("Didn't get coordinates with error: " + err.code);
}
function locationSuccess(position) {
    // can manually set coordinates here for testing...
    //position.coords.latitude = 55.35;
    //position.coords.longitude = 23.67;
    // Vilnius:
    //position.coords.latitude = 55.35;
    //position.coords.longitude = 23.67;
  
    console.log("Got latitude:  " + position.coords.latitude);
    console.log("Got longitude: " + position.coords.longitude);

    Pebble.sendAppMessage( { "latitude": position.coords.latitude.toString(),
      "longitude": position.coords.longitude.toString() },
      function(e) { console.log("Successfully delivered message with transactionId="   + e.data.transactionId); },
      function(e) { console.log("Unsuccessfully delivered message with transactionId=" + e.data.transactionId);}
    );
}

function sendTimezoneToWatch() {
  // Get the number of seconds to add to convert localtime to utc
  var offsetMinutes = new Date().getTimezoneOffset() * 60;
  // Send it to the watch
  Pebble.sendAppMessage({ "timezoneOffsetJS": offsetMinutes },
    function(e) { console.log("Successfully sent timezoneOffsetJS="+ offsetMinutes +", transactionId="   + e.data.transactionId); },
    function(e) { console.log("Failed sending timezoneOffsetJS with transactionId=" + e.data.transactionId);});
}

Pebble.addEventListener("appmessage", function(e) {
    console.log("Received from phone: " + JSON.stringify(e.payload));
});

Pebble.addEventListener("showConfiguration", function(e) {
    Pebble.openURL(config_url);
});

Pebble.addEventListener("webviewclosed", function(e) {
    console.log("Configuration closed");
    console.log(e.response);
    if (e.response) {
      var options = JSON.parse(decodeURIComponent(e.response));
      console.log("Options = " + JSON.stringify(options));
      Pebble.sendAppMessage( options );
    }
    else {
      console.log("User clicked cancel.");
    }
});
