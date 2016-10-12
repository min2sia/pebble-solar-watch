//TODO: send pre-formatted date string, e.g. "Oct 1"
//TODO: weather update on?..

console.log('JS started');

var months = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];

// var watchInfo = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
// if (watchInfo) {
//     console.log(JSON.stringify(watchInfo));
// }

//TODO: https://github.com/mourner/suncalc?
var SolarCalc = require('solar-calc');

// var Clay = require('pebble-clay');
// var clayConfig = require('./config');
// var clay = new Clay(clayConfig);

var locationOptions = {
  enableHighAccuracy: false,
  maximumAge: Infinity,
  timeout: 10000
};

var latitude;
var longitude;
var lastLatitude;
var lastLongitude;
var temperatureUnits = 'metric'; //TODO: configurable
var applicationStarting = true;
var message = {};

Pebble.addEventListener("ready",
    function(e) {
        console.log("Event 'ready'");// + JSON.stringify(e));    
     
        latitude  = localStorage.getItem("latitude");
        longitude = localStorage.getItem("longitude");
        lastLatitude  = latitude;
        lastLongitude = longitude;

        console.log("Retrieved from localStorage: latitude=" + latitude + ", longitude=" + longitude);
        
        // Get current or cached location when starting:
        //navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
        
        // Subscribe to location updates
        navigator.geolocation.watchPosition(locationUpdateSuccess, locationError, locationOptions);
    }
);

Pebble.addEventListener('appmessage',
    function(e) {
        console.log('Received appmessage');// + JSON.stringify(e));
        
        // Incomming communication from watch currently only used to trigger location refresh:
        //navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }
);

// to make sure nothing else but numbers is passed in the AppMessage
function toNumber(anyValueIn)
{
    var numValueOut = Number(anyValueIn);
    return isNaN(numValueOut) ? 0 : numValueOut;
}

function sendToWatch() {
    console.log("sendToWatch()");
    
//     if (!sunrise || !sunset) {
//         console.log("  Error: sun data not initialized");
//         return;
//     }
//     console.log("  solarOffset = " + solarOffset);
//     console.log("  sunrise     = " + sunrise.getHours() + ":" + sunrise.getMinutes());
//     console.log("  sunset      = " + sunset.getHours()  + ":" + sunset.getMinutes());
        
    //TODO: get phone's locale?
    //message.DATE = new Date().toLocaleString('en', {month:'short', day:'numeric'});
    var date = new Date();
    message.DATE = months[date.getMonth()] + ' ' + date.getDate();
    
    Pebble.sendAppMessage(
        message, 
        function(e) { 
            console.log("Successfully delivered message"); //   + JSON.stringify(e)); 
            message = {};
        },
        function(e) { 
            console.log("Message delivery failed"); // + JSON.stringify(e));
            message = {};
        }
    );
}

// function locationSuccess(position) {
//     latitude  = position.coords.latitude;
//     longitude = position.coords.longitude;
//     console.log("Got location: lat " + latitude + ", lon " + longitude);
//     //Pebble.showSimpleNotificationOnPebble('Got location', 'Lat ' + latitude + ', long ' + longitude);
    
//     localStorage.setItem("latitude",  latitude);
//     localStorage.setItem("longitude", longitude);
    
//     fetchWeather(latitude, longitude);
    
//     if (applicationStarting) {
//         calculateSunData();   
//         sendToWatch();    
//         applicationStarting = false;
//     } else if (Math.abs(latitude - lastLatitude) > 0.25 || Math.abs(longitude - lastLongitude) > 0.25) { // if location change is significant
//         calculateSunData();   
//         sendToWatch();
//         lastLatitude  = latitude;
//         lastLongitude = longitude;
//     }
// }

function locationUpdateSuccess(position) {
    latitude  = position.coords.latitude;
    longitude = position.coords.longitude;
    console.log("Location updated: lat " + latitude + ", lon " + longitude);
    
    localStorage.setItem("latitude",  latitude);
    localStorage.setItem("longitude", longitude);
    
    if (applicationStarting) {
        calculateSunData();   
        fetchWeather(latitude, longitude);
        //sendToWatch();    
        applicationStarting = false;
    } else if (Math.abs(latitude - lastLatitude) > 0.25 || Math.abs(longitude - lastLongitude) > 0.25) { // if location change is significant
        Pebble.showSimpleNotificationOnPebble('Location updated by', 'Lat ' + (latitude - lastLatitude) + ', lon ' + (longitude - lastLongitude));
        calculateSunData();   
        //sendToWatch();  
        lastLatitude  = latitude;
        lastLongitude = longitude;
        fetchWeather(latitude, longitude);
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
        fetchWeather(latitude, longitude);
        applicationStarting = false;
    }
}

function calculateSunData() {

    //TODO: testing location
    //latitude = 55.348393;
    //longitude = 23.663658;
    
    if (latitude !== null && longitude !== null) {
        var solarCalc = new SolarCalc(new Date(), latitude, longitude);
        var sunrise           = solarCalc.sunrise;
        var sunset            = solarCalc.sunset;
        var civilDawn         = solarCalc.civilDawn;
        var nauticalDawn      = solarCalc.nauticalDawn;
        var astronomicalDawn  = solarCalc.astronomicalDawn;
        var civilDusk         = solarCalc.civilDusk;
        var nauticalDusk      = solarCalc.nauticalDusk;
        var astronomicalDusk  = solarCalc.astronomicalDusk;
        var goldenHourEvening = solarCalc.goldenHourStart;
        var goldenHourMorning = solarCalc.goldenHourEnd;
        var solarNoon         = solarCalc.solarNoon;

        var zoneNoon = new Date(solarNoon); 
        zoneNoon.setHours(12, 0, 0);
        var solarOffset = Math.floor((zoneNoon.getTime() - solarNoon.getTime()) / 1000);
        
        message.SOLAR_OFFSET               = toNumber(solarOffset);
        message.SUNRISE_HOURS              = toNumber(sunrise.getHours());
        message.SUNRISE_MINUTES            = toNumber(sunrise.getMinutes());
        message.CIVIL_DAWN_HOURS           = toNumber(civilDawn.getHours());
        message.CIVIL_DAWN_MINUTES         = toNumber(civilDawn.getMinutes());
        message.NAUTICAL_DAWN_HOURS        = toNumber(nauticalDawn.getHours());
        message.NAUTICAL_DAWN_MINUTES      = toNumber(nauticalDawn.getMinutes());
        message.ASTRONOMICAL_DAWN_HOURS    = toNumber(astronomicalDawn.getHours());
        message.ASTRONOMICAL_DAWN_MINUTES  = toNumber(astronomicalDawn.getMinutes());
        message.GOLDEN_H_MORNING_HOURS     = toNumber(goldenHourMorning.getHours());
        message.GOLDEN_H_MORNING_MINUTES   = toNumber(goldenHourMorning.getMinutes());
        message.SUNSET_HOURS               = toNumber(sunset.getHours());
        message.SUNSET_MINUTES             = toNumber(sunset.getMinutes());
        message.CIVIL_DUSK_HOURS           = toNumber(civilDusk.getHours());
        message.CIVIL_DUSK_MINUTES         = toNumber(civilDusk.getMinutes());
        message.NAUTICAL_DUSK_HOURS        = toNumber(nauticalDusk.getHours());
        message.NAUTICAL_DUSK_MINUTES      = toNumber(nauticalDusk.getMinutes());
        message.ASTRONOMICAL_DUSK_HOURS    = toNumber(astronomicalDusk.getHours());
        message.ASTRONOMICAL_DUSK_MINUTES  = toNumber(astronomicalDusk.getMinutes());
        message.GOLDEN_H_EVENING_HOURS     = toNumber(goldenHourEvening.getHours());
        message.GOLDEN_H_EVENING_MINUTES   = toNumber(goldenHourEvening.getMinutes());
    }    
}

function fetchWeather(latitude, longitude) {
    var url = 'http://api.openweathermap.org/data/2.5/weather?' + 
              'units=' + temperatureUnits +
              '&lat=' + latitude + 
              '&lon=' + longitude + 
              '&cnt=1'+
              '&appid=2b5f53285b1d3978482a0ad6888d0427';
    //console.log('fetchWeather(): ' + url);
    
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
    req.onload = function () {
        //console.log('req.onload()');
        if (req.readyState === 4) {
            if (req.status === 200) {
                //console.log(req.responseText);
                var response = JSON.parse(req.responseText);
                var temperature = Math.round(response.main.temp);
                //console.log('Got temperature: ' + temperature);                
                message.TEMPERATURE = temperature + '\xB0'; 
            } else {
                console.log('ERROR: unable to fetch weather info ' + JSON.stringify(req));
            }
        }
        sendToWatch();
    };
    req.send();
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
