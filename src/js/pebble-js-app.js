//var config_url = "http://mtc.nfshost.com/sunset-watch-config.html";
var latitude;
var longitude;
var solarOffset;
var sunrise;
var sunset;
var applicationStarting = true;

// There are two cases when data is sent to watch:
// 1. On application startup. If location service is not available cached location is used.
// 2. Periodic update. Data is sent only if location is available and location has changed considerably

function requestLocationAsync() {
    console.log("requestLocationAsync()");
    navigator.geolocation.getCurrentPosition(
        locationSuccessLo,
        locationErrorLo,
        {
            enableHighAccuracy: false, // default
            timeout: 10000, 
            maximumAge: Infinity
        }
    );
    
    navigator.geolocation.getCurrentPosition(
        locationSuccessHi,
        locationErrorHi,
        {
            enableHighAccuracy: true, // default
            timeout: 10000, 
            maximumAge: Infinity
        }
    );
}

Pebble.addEventListener("ready",
    function(e) {
        console.log("JS starting...");
            
//        latitude  = localStorage.getItem("latitude");
//        longitude = localStorage.getItem("longitude");
//        console.log("Retrieved from localStorage: latitude=" + latitude + ", longitude=" + longitude);
        
        // Call first time when starting:
        requestLocationAsync();
        
        // Schedule periodic position poll every X minutes:
        setInterval(function() {requestLocationAsync();}, 30*1000); //15*60*1000); 
    }
);

Pebble.addEventListener('appmessage',
    function(e) {
        console.log('Received appmessage: ' + JSON.stringify(e.payload));
        
        requestLocationAsync(); 
    }
);

function sendToWatch() {
    console.log("Sending sun data:");
        console.log("  solarOffset = " + solarOffset);
        console.log("  sunrise     = " + sunrise.getHours() + ":" + sunrise.getMinutes());
        console.log("  sunset      = " + sunset.getHours()  + ":" + sunset.getMinutes());
    
    Pebble.sendAppMessage( 
        {"solarOffset"    : solarOffset,
         "sunriseHours"   : sunrise.getHours(),
         "sunriseMinutes" : sunrise.getMinutes(),
         "sunsetHours"    : sunset.getHours(),
         "sunsetMinutes"  : sunset.getMinutes()},
      function(e) { console.log("Successfully delivered message with transactionId="   + e.data.transactionId); },
      function(e) { console.log("Unsuccessfully delivered message with transactionId=" + e.data.transactionId);}
    );
}

function locationSuccessLo(position) {
    console.log("Location success - Low accuracy");
    locationSuccess(position);
}
function locationSuccessHi(position) {
    console.log("Location success - High accuracy");
    locationSuccess(position);
}
function locationSuccess(position) {
    var lastLatitude  = latitude;
    var lastLongitude = longitude;
    
    latitude  = position.coords.latitude;
    longitude = position.coords.longitude;
    console.log("Got position: lat " + latitude + ", long " + longitude);
    
    localStorage.setItem("latitude",  latitude);
    localStorage.setItem("longitude", longitude);
    
    if (applicationStarting) {
        calculateSunData();   
        sendToWatch();    
        applicationStarting = false;
    } else if (true)
//Math.abs(latitude - lastLatitude) > 0.01 || Math.abs(longitude - lastLongitude) > 0.01) { // if location change is significant
        calculateSunData();   
        sendToWatch();  
    }
    
}

function locationErrorLo(error) {
    console.log("Location error - Low accuracy");
    locationError(error);
}
function locationErrorHi(error) {
    console.log("Location error - High accuracy");
    locationError(error);
}
function locationError(error) {
    console.log("navigator.geolocation.getCurrentPosition() returned error " + error.code);
    switch(error.code) {
        case error.PERMISSION_DENIED:
            console.log("  Permission denied.");
            break;
        case error.POSITION_UNAVAILABLE:
            console.log("  Position unavailable.");
            break;
        case error.TIMEOUT:
            console.log("  Timeout.");
            break;
        case error.UNKNOWN_ERROR:
            console.log("  Unknown error.");
            break;
    }    
    
    if (applicationStarting) {
        // use last cached location if available
        calculateSunData();   
        sendToWatch();
        applicationStarting = false;
    }
}

function calculateSunData() {
    // Test location:
    //latitude  = 54.69922
    //longitude = 25.213801;
    
    if (latitude && longitude) {
        var times = SunCalc.getTimes(new Date(), latitude, longitude);
        sunrise = times.sunrise;
        sunset = times.sunset;
        var solarNoon = times.solarNoon;
        var zoneNoon  = new Date(solarNoon); 
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

/*------------------------------------------------------------------------------------
 (c) 2011-2014, Vladimir Agafonkin
 SunCalc is a JavaScript library for calculating sun/mooon position and light phases.
 https://github.com/mourner/suncalc
*/

// shortcuts for easier to read formulas

var PI   = Math.PI,
    sin  = Math.sin,
    cos  = Math.cos,
    tan  = Math.tan,
    asin = Math.asin,
    atan = Math.atan2,
    acos = Math.acos,
    rad  = PI / 180;

// sun calculations are based on http://aa.quae.nl/en/reken/zonpositie.html formulas


// date/time constants and conversions

var dayMs = 1000 * 60 * 60 * 24,
    J1970 = 2440588,
    J2000 = 2451545;

function toJulian(date) { return date.valueOf() / dayMs - 0.5 + J1970; }
function fromJulian(j)  { return new Date((j + 0.5 - J1970) * dayMs); }
function toDays(date)   { return toJulian(date) - J2000; }


// general calculations for position

var e = rad * 23.4397; // obliquity of the Earth

function rightAscension(l, b) { return atan(sin(l) * cos(e) - tan(b) * sin(e), cos(l)); }
function declination(l, b)    { return asin(sin(b) * cos(e) + cos(b) * sin(e) * sin(l)); }

function azimuth(H, phi, dec)  { return atan(sin(H), cos(H) * sin(phi) - tan(dec) * cos(phi)); }
function altitude(H, phi, dec) { return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H)); }

function siderealTime(d, lw) { return rad * (280.16 + 360.9856235 * d) - lw; }


// general sun calculations

function solarMeanAnomaly(d) { return rad * (357.5291 + 0.98560028 * d); }

function eclipticLongitude(M) {

    var C = rad * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M)), // equation of center
        P = rad * 102.9372; // perihelion of the Earth

    return M + C + P + PI;
}

function sunCoords(d) {

    var M = solarMeanAnomaly(d),
        L = eclipticLongitude(M);

    return {
        dec: declination(L, 0),
        ra: rightAscension(L, 0)
    };
}


var SunCalc = {};


// calculates sun position for a given date and latitude/longitude

SunCalc.getPosition = function (date, lat, lng) {

    var lw  = rad * -lng,
        phi = rad * lat,
        d   = toDays(date),

        c  = sunCoords(d),
        H  = siderealTime(d, lw) - c.ra;

    return {
        azimuth: azimuth(H, phi, c.dec),
        altitude: altitude(H, phi, c.dec)
    };
};


// sun times configuration (angle, morning name, evening name)

var times = SunCalc.times = [
    [-0.833, 'sunrise',       'sunset'      ],
    [  -0.3, 'sunriseEnd',    'sunsetStart' ],
    [    -6, 'dawn',          'dusk'        ],
    [   -12, 'nauticalDawn',  'nauticalDusk'],
    [   -18, 'nightEnd',      'night'       ],
    [     6, 'goldenHourEnd', 'goldenHour'  ]
];

// adds a custom time to the times config

SunCalc.addTime = function (angle, riseName, setName) {
    times.push([angle, riseName, setName]);
};


// calculations for sun times

var J0 = 0.0009;

function julianCycle(d, lw) { return Math.round(d - J0 - lw / (2 * PI)); }

function approxTransit(Ht, lw, n) { return J0 + (Ht + lw) / (2 * PI) + n; }
function solarTransitJ(ds, M, L)  { return J2000 + ds + 0.0053 * sin(M) - 0.0069 * sin(2 * L); }

function hourAngle(h, phi, d) { return acos((sin(h) - sin(phi) * sin(d)) / (cos(phi) * cos(d))); }

// returns set time for the given sun altitude
function getSetJ(h, lw, phi, dec, n, M, L) {

    var w = hourAngle(h, phi, dec),
        a = approxTransit(w, lw, n);
    return solarTransitJ(a, M, L);
}


// calculates sun times for a given date and latitude/longitude

SunCalc.getTimes = function (date, lat, lng) {

    var lw = rad * -lng,
        phi = rad * lat,

        d = toDays(date),
        n = julianCycle(d, lw),
        ds = approxTransit(0, lw, n),

        M = solarMeanAnomaly(ds),
        L = eclipticLongitude(M),
        dec = declination(L, 0),

        Jnoon = solarTransitJ(ds, M, L),

        i, len, time, Jset, Jrise;


    var result = {
        solarNoon: fromJulian(Jnoon),
        nadir: fromJulian(Jnoon - 0.5)
    };

    for (i = 0, len = times.length; i < len; i += 1) {
        time = times[i];

        Jset = getSetJ(time[0] * rad, lw, phi, dec, n, M, L);
        Jrise = Jnoon - (Jset - Jnoon);

        result[time[1]] = fromJulian(Jrise);
        result[time[2]] = fromJulian(Jset);
    }

    return result;
};