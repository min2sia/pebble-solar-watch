var SolarCalc = require('solar-calc');

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var locationOptions = {
  enableHighAccuracy: false,
  maximumAge: Infinity,
  timeout: 10000
};

var latitude;
var longitude;
var lastLatitude;
var lastLongitude;
var watchId;
var solarOffset;
var sunrise;
var sunset;
var civilDawn;
var nauticalDawn;
var astronomicalDawn;
var civilDusk;
var nauticalDusk;
var astronomicalDusk;
var goldenHourMorning;
var goldenHourEvening;

var applicationStarting = true;

Pebble.addEventListener("ready",
    function(e) {
        console.log("JS starting...");
        console.log(e.type);      
     
        latitude  = localStorage.getItem("latitude");
        longitude = localStorage.getItem("longitude");
        lastLatitude  = latitude;
        lastLongitude = longitude;

        console.log("Retrieved from localStorage: latitude=" + latitude + ", longitude=" + longitude);
        
        // Get current or cached location when starting:
        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
        
        // Subscribe to location updates
        watchId = navigator.geolocation.watchPosition(locationUpdateSuccess, locationError, locationOptions);
    }
);

Pebble.addEventListener('appmessage',
    function(e) {
        console.log('Received appmessage: ' + JSON.stringify(e.payload));
        
        // Incomming communication from watch currently only used to trigger location refresh:
        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }
);

function sendToWatch() {
    console.log("Sending sun data:");
        console.log("  solarOffset = " + solarOffset);
        console.log("  sunrise     = " + sunrise.getHours() + ":" + sunrise.getMinutes());
        console.log("  sunset      = " + sunset.getHours()  + ":" + sunset.getMinutes());
    
    Pebble.sendAppMessage( 
        {"SOLAR_OFFSET"               : solarOffset,
         "SUNRISE_HOURS"              : sunrise.getHours(),
         "SUNRISE_MINUTES"            : sunrise.getMinutes(),
         "SUNSET_HOURS"               : sunset.getHours(),
         "SUNSET_MINUTES"             : sunset.getMinutes(),
         "CIVIL_DAWN_HOURS"           : civilDawn.getHours(),
         "CIVIL_DAWN_MINUTES"         : civilDawn.getMinutes(),
         "NAUTICAL_DAWN_HOURS"        : nauticalDawn.getHours(),
         "NAUTICAL_DAWN_MINUTES"      : nauticalDawn.getMinutes(),
         "ASTRONOMICAL_DAWN_HOURS"    : astronomicalDawn.getHours(),
         "ASTRONOMICAL_DAWN_MINUTES"  : astronomicalDawn.getMinutes(),
         "CIVIL_DUSK_HOURS"           : civilDusk.getHours(),
         "CIVIL_DUSK_MINUTES"         : civilDusk.getMinutes(),
         "NAUTICAL_DUSK_HOURS"        : nauticalDusk.getHours(),
         "NAUTICAL_DUSK_MINUTES"      : nauticalDusk.getMinutes(),
         "ASTRONOMICAL_DUSK_HOURS"    : astronomicalDusk.getHours(),
         "ASTRONOMICAL_DUSK_MINUTES"  : astronomicalDusk.getMinutes(),
         "GOLDEN_H_MORNING_HOURS"     : goldenHourMorning.getHours(),
         "GOLDEN_H_MORNING_MINUTES"   : goldenHourMorning.getMinutes(),
         "GOLDEN_H_EVENING_HOURS"     : goldenHourEvening.getHours(),
         "GOLDEN_H_EVENING_MINUTES"   : goldenHourEvening.getMinutes(),         
        },
      function(e) { console.log("Successfully delivered message with transactionId="   + e.data.transactionId); },
      function(e) { console.log("Unsuccessfully delivered message with transactionId=" + e.data.transactionId);}
    );

}

function locationSuccess(position) {
    latitude  = position.coords.latitude;
    longitude = position.coords.longitude;
    console.log("Got location: lat " + latitude + ", long " + longitude);
    //Pebble.showSimpleNotificationOnPebble('Got location', 'Lat ' + latitude + ', long ' + longitude);
    
    localStorage.setItem("latitude",  latitude);
    localStorage.setItem("longitude", longitude);
    
    if (applicationStarting) {
        calculateSunData();   
        sendToWatch();    
        applicationStarting = false;
    } else if (Math.abs(latitude - lastLatitude) > 0.25 || Math.abs(longitude - lastLongitude) > 0.25) { // if location change is significant
        calculateSunData();   
        sendToWatch();
        lastLatitude  = latitude;
        lastLongitude = longitude;

    }
}

function locationUpdateSuccess(position) {
    latitude  = position.coords.latitude;
    longitude = position.coords.longitude;
    console.log("Location updated: lat " + latitude + ", long " + longitude);
    
    localStorage.setItem("latitude",  latitude);
    localStorage.setItem("longitude", longitude);
    
    if (applicationStarting) {
        calculateSunData();   
        sendToWatch();    
        applicationStarting = false;
    } else if (Math.abs(latitude - lastLatitude) > 0.25 || Math.abs(longitude - lastLongitude) > 0.25) { // if location change is significant
        Pebble.showSimpleNotificationOnPebble('Location updated by', 'Lat ' + (latitude - lastLatitude) + ', long ' + (longitude - lastLongitude));
        calculateSunData();   
        sendToWatch();  
        lastLatitude  = latitude;
        lastLongitude = longitude;
    }
}

function locationError(error) {
    var errorText;
    
    switch(error.code) {
        case error.PERMISSION_DENIED:
            errorText = "Permission denied";
            break;
        case error.POSITION_UNAVAILABLE:
            errorText = "Position unavailable";
            break;
        case error.TIMEOUT:
            errorText = "Timeout";
            break;
        case error.UNKNOWN_ERROR:
            errorText = "Unknown error";
            break;
    }    
    
    console.log("navigator.geolocation.getCurrentPosition() returned error " + error.code + ": " + errorText);
    
    if (applicationStarting) {
        // use last cached location if available
        calculateSunData();   
        sendToWatch();
        applicationStarting = false;
    }
}

function calculateSunData() {

    //TODO: testing location
    //latitude = 53.353;
    //longitude = -6.4565011;
    
    if (latitude && longitude) {
        var solarCalc = new SolarCalc(new Date(), latitude, longitude);
        sunrise           = solarCalc.sunrise;
        sunset            = solarCalc.sunset;
        civilDawn         = solarCalc.civilDawn;
        nauticalDawn      = solarCalc.nauticalDawn;
        astronomicalDawn  = solarCalc.astronomicalDawn;
        civilDusk         = solarCalc.civilDusk;
        nauticalDusk      = solarCalc.nauticalDusk;
        astronomicalDusk  = solarCalc.astronomicalDusk;
        goldenHourMorning = solarCalc.goldenHourStart;
        goldenHourEvening = solarCalc.goldenHourEnd;
        var solarNoon     = solarCalc.solarNoon;
        
        var zoneNoon     = new Date(solarNoon); 
        zoneNoon.setHours(12, 0, 0);
        solarOffset = Math.floor((zoneNoon.getTime() - solarNoon.getTime()) / 1000);
    }    
}

//Pebble.addEventListener("showConfiguration", function(e) {
//    Pebble.openURL(config_url);
//});

//Pebble.addEventListener("webviewclosed", function(e) {
//    console.log("Configuration closed");
//    console.log(e.response);
//    if (e.response) {
//      var options = JSON.parse(decodeURIComponent(e.response));
//      console.log("Options = " + JSON.stringify(options));
//      Pebble.sendAppMessage( options );
//    }
//    else {
//      console.log("User clicked cancel.");
//    }
//});
