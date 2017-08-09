// console.log('JS started');

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
  //enableHighAccuracy: false,
  maximumAge: 60000,
  timeout: 15000
};

var appIsStarting = true;
var latitude = null;
var longitude = null;
// var lastLatitude;
// var lastLongitude;
// var watchPositionId;
var weatherUnitSystem = 'metric'; //TODO: configurable

Pebble.addEventListener("ready",
    function(e) {
//         console.log("Event 'ready'");// + JSON.stringify(e));   
        sendToWatch({'JSREADY': 1});
        
        //localStorage.clear();
        
        latitude  = toNumber(localStorage.getItem("latitude"));
        longitude = toNumber(localStorage.getItem("longitude"));

//         console.log("Location from localStorage: " + latitude + ", " + longitude);
        
        updateSunData();   
        updateWeather(); 

        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }
);

// Incomming communication from watch currently only used to trigger refresh
Pebble.addEventListener('appmessage',
    function(e) {
//         console.log('Received appmessage');// + JSON.stringify(e));
        
        navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
    }
);

// to make sure nothing else but numbers is passed in the AppMessage
function toNumber(anyValueIn)
{
    var numValueOut = Number(anyValueIn);
    return isNaN(numValueOut) ? 0 : numValueOut;
}

function sendToWatch(message) {
    //TODO: get phone's locale?
    //message.DATE = new Date().toLocaleString('en', {month:'short', day:'numeric'});
    
    var date = new Date();
    message.DATE = months[date.getMonth()] + ' ' + date.getDate();

//     console.log("sendToWatch():");
//     console.log("  " + JSON.stringify(message));
    
    Pebble.sendAppMessage(
        message, 
        function(e) { 
//             console.log("Successfully delivered message"); //   + JSON.stringify(e)); 
        },
        function(e) { 
//             console.log("Message delivery failed"); // + JSON.stringify(e));
        }
    );
}

function locationSuccess(position) {
    sendToWatch({"LOCATION_AVAILABLE" : 1});
//     console.log("Got location: " + position.coords.latitude + ", " + position.coords.longitude);

    latitude  = toNumber(position.coords.latitude);
    longitude = toNumber(position.coords.longitude);
//latitude = 70; //54;
//longitude = 25;
    localStorage.setItem("latitude",  latitude);
    localStorage.setItem("longitude", longitude);
//     if (lastLatitude === null) {
//         lastLatitude = latitude; 
//     }
//     if (lastLongitude === null) {
//         lastLongitude = longitude;
//     }

//     if (Math.abs(latitude - lastLatitude) > 0.25 || Math.abs(longitude - lastLongitude) > 0.25) { // if location change is significant
//         //Pebble.showSimpleNotificationOnPebble('Location updated by', 'Lat ' + (latitude - lastLatitude) + ', lon ' + (longitude - lastLongitude));
//         updateSunData();
//         updateWeather();
//         lastLatitude  = latitude;
//         lastLongitude = longitude;
//     }   
    updateSunData();
    updateWeather();
}

function locationError(error) {
    sendToWatch({"LOCATION_AVAILABLE" : 0});

    // Updade with cached location
    if (appIsStarting) {
        appIsStarting = false;    
        updateSunData();
    } else {
        updateWeather();         
    }
    
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
//     console.log("navigator.geolocation.getCurrentPosition() returned error " + error.code + ": " + errorText);
}

function updateSunData() {
    var message = {};
    
    if (latitude === null || longitude === null) {
        return;
    }
    
    var solarCalc = new SolarCalc(new Date(), latitude, longitude);
    var solarNoon         = solarCalc.solarNoon;
    var zoneNoon = new Date(solarNoon); 
    zoneNoon.setHours(12, 0, 0);
    var solarOffsetMs = Math.floor((zoneNoon.getTime() - solarNoon.getTime()) / 1000);
    var solarOffset = zoneNoon.getTime() - solarNoon.getTime();

    var w_sunrise                    = new Date(solarCalc.sunrise.getTime());
    var s_sunrise                    = new Date(solarCalc.sunrise.getTime());
// console.log("solarCalc.sunrise: " + solarCalc.sunrise);
// console.log("solarCalc.sunrise.getTime(): " + solarCalc.sunrise.getTime());
// console.log("w_sunrise: " + w_sunrise);
    var w_sunset            = new Date(solarCalc.sunset.getTime());
    var s_sunset            = new Date(solarCalc.sunset.getTime());
    var w_civilDawn         = new Date(solarCalc.civilDawn.getTime());
    var s_civilDawn         = new Date(solarCalc.civilDawn.getTime());
    var w_civilDusk         = new Date(solarCalc.civilDusk.getTime());
    var s_civilDusk         = new Date(solarCalc.civilDusk.getTime());
    var w_nauticalDawn      = new Date(solarCalc.nauticalDawn.getTime());
    var s_nauticalDawn      = new Date(solarCalc.nauticalDawn.getTime());
    var w_nauticalDusk      = new Date(solarCalc.nauticalDusk.getTime());
    var s_nauticalDusk      = new Date(solarCalc.nauticalDusk.getTime());
    var w_astronomicalDawn  = new Date(solarCalc.astronomicalDawn.getTime());
    var s_astronomicalDawn  = new Date(solarCalc.astronomicalDawn.getTime());
    var w_astronomicalDusk  = new Date(solarCalc.astronomicalDusk.getTime());
    var s_astronomicalDusk  = new Date(solarCalc.astronomicalDusk.getTime());
//     var goldenHourEvening = solarCalc.goldenHourStart;
//     var goldenHourMorning = solarCalc.goldenHourEnd;

    message.SOLAR_OFFSET               = toNumber(solarOffsetMs);

    s_sunrise.setMilliseconds         (w_sunrise.getMilliseconds()          + solarOffset);
    s_sunset.setMilliseconds          (w_sunset.getMilliseconds()           + solarOffset);
    s_civilDawn.setMilliseconds       (w_civilDawn.getMilliseconds()        + solarOffset);
    s_civilDusk.setMilliseconds       (w_civilDusk.getMilliseconds()        + solarOffset);
    s_nauticalDawn.setMilliseconds    (w_nauticalDawn.getMilliseconds()     + solarOffset);
    s_nauticalDusk.setMilliseconds    (w_nauticalDusk.getMilliseconds()     + solarOffset);
    s_astronomicalDawn.setMilliseconds(w_astronomicalDawn.getMilliseconds() + solarOffset);
    s_astronomicalDusk.setMilliseconds(w_astronomicalDusk.getMilliseconds() + solarOffset);

console.log("------ Wall  ------");
console.log("Sunrise:           " + w_sunrise          + " set:  " + w_sunset);
console.log("Civil dawn:        " + w_civilDawn        + " dusk: " + w_civilDusk);
console.log("Nautical dawn:     " + w_nauticalDawn     + " dusk: " + w_nauticalDusk);
console.log("Astronomical dawn: " + w_astronomicalDawn + " dusk: " + w_astronomicalDusk);
console.log("------ Solar ------");
console.log("Sunrise:           " + s_sunrise          + " set:  " + s_sunset);
console.log("Civil dawn:        " + s_civilDawn        + " dusk: " + s_civilDusk);
console.log("Nautical dawn:     " + s_nauticalDawn     + " dusk: " + s_nauticalDusk);
console.log("Astronomical dawn: " + s_astronomicalDawn + " dusk: " + s_astronomicalDusk);
    
    function isSameDay(date1, date2) {
        if (date1.getDate()     !== date2.getDate()  ||
            date1.getMonth()    !== date2.getMonth() ||
            date1.getFullYear() !== date2.getFullYear()) {
            return false;
        } else {
            return true;
        }
    }
    if (isSameDay(s_sunrise, s_sunset)) {
        message.W_SUNRISE_HOURS            = toNumber(w_sunrise.getHours());
        message.W_SUNRISE_MINUTES          = toNumber(w_sunrise.getMinutes());
        message.W_SUNSET_HOURS             = toNumber(w_sunset.getHours());
        message.W_SUNSET_MINUTES           = toNumber(w_sunset.getMinutes());   
        message.SUNRISE_HOURS              = toNumber(s_sunrise.getHours());
        message.SUNRISE_MINUTES            = toNumber(s_sunrise.getMinutes());
        message.SUNSET_HOURS               = toNumber(s_sunset.getHours());
        message.SUNSET_MINUTES             = toNumber(s_sunset.getMinutes());                
        message.POLAR_DAY_NIGHT            = 0;
    } else if (s_sunrise.getTime() < s_sunset.getTime()) {
        console.log("POLAR DAY");        
        message.POLAR_DAY_NIGHT            = 1;
    } else if (s_sunrise.getTime() > s_sunset.getTime()) {
        console.log("POLAR NIGHT");
        message.POLAR_DAY_NIGHT            = 2;        
    }
    
    if (isSameDay(s_civilDawn, s_civilDusk)) {
        message.CIVIL_DAWN_HOURS           = toNumber(s_civilDawn.getHours());
        message.CIVIL_DAWN_MINUTES         = toNumber(s_civilDawn.getMinutes());
        message.CIVIL_DUSK_HOURS           = toNumber(s_civilDusk.getHours());
        message.CIVIL_DUSK_MINUTES         = toNumber(s_civilDusk.getMinutes());        
    }
    if (isSameDay(s_nauticalDawn, s_nauticalDusk)) {
        message.NAUTICAL_DAWN_HOURS        = toNumber(s_nauticalDawn.getHours());
        message.NAUTICAL_DAWN_MINUTES      = toNumber(s_nauticalDawn.getMinutes());
        message.NAUTICAL_DUSK_HOURS        = toNumber(s_nauticalDusk.getHours());
        message.NAUTICAL_DUSK_MINUTES      = toNumber(s_nauticalDusk.getMinutes());        
    }
    if (isSameDay(s_astronomicalDawn, s_astronomicalDusk)) {
        message.ASTRONOMICAL_DAWN_HOURS    = toNumber(s_astronomicalDawn.getHours());
        message.ASTRONOMICAL_DAWN_MINUTES  = toNumber(s_astronomicalDawn.getMinutes());
        message.ASTRONOMICAL_DUSK_HOURS    = toNumber(s_astronomicalDusk.getHours());
        message.ASTRONOMICAL_DUSK_MINUTES  = toNumber(s_astronomicalDusk.getMinutes());
    }
    //         message.GOLDEN_H_MORNING_HOURS     = toNumber(goldenHourMorning.getHours());
    //         message.GOLDEN_H_MORNING_MINUTES   = toNumber(goldenHourMorning.getMinutes());
    //         message.GOLDEN_H_EVENING_HOURS     = toNumber(goldenHourEvening.getHours());
    //         message.GOLDEN_H_EVENING_MINUTES   = toNumber(goldenHourEvening.getMinutes());
    
    sendToWatch(message);
}

function updateWeather() {
//     console.log('updateWeather(): latitude='+latitude+', longitude='+longitude);
    if (!latitude || !longitude) {
        return;
    }
    
    var url = 'http://api.openweathermap.org/data/2.5/weather?' + 
              'units=' + weatherUnitSystem +
              '&lat=' + latitude + 
              '&lon=' + longitude + 
              '&cnt=1'+
              '&appid=2b5f53285b1d3978482a0ad6888d0427';
//     console.log('updateWeather(): ' + url);
    
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
    req.onload = function () {
//         console.log('req.onload()');
        if (req.readyState === 4) {
            if (req.status === 200) {
//                 console.log(req.responseText);
                var response = JSON.parse(req.responseText);
                var temperature = Math.round(response.main.temp);
                var country = response.sys.country;
                if (country === 'US') {
                    temperature = temperature * 9 / 5 + 32; // convert to Fahrenheit
                }
                console.log('Got temperature: ' + temperature + ' (' + country + ')');    
                sendToWatch({"TEMPERATURE" : temperature + '\xB0'});
            } else {
                console.log('ERROR: unable to fetch weather info ' + JSON.stringify(req));
            }
        }
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
