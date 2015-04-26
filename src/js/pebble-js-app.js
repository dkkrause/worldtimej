var getlocationOptions = {
                           "timeout": 15000,
                           "maximumAge": 1800000
                         };

// var watchlocationOptions = {
//                              "timeout": 15000,
//                              "maximumAge": 1800000
//                            };

var pbl_msg_queue = [];
var appData = {};
var fioKeyString = "fioKey";

/*
 * Get the default values, if they exist. If not, initialize them.
 */
function getDefaults() {
    var defString;
    var tmpDefaults = {};
    var defaults = [];
    var tmpFioKey;
//    localStorage.clear();
    tmpFioKey = localStorage.getItem(fioKeyString);
    if (!tmpFioKey) tmpFioKey = "";
    for (var i = 0; i < 3; i++ ) {
        defString = localStorage.getItem("defaults" + i);
        if ( defString === null ) {
            tmpDefaults = {
                            "background" : 0,
                            "timedisp"   : 0,
                            "latitude"   : (i === 0) ? 0 : (i === 1) ? 51.507200 :  35.689500,
                            "longitude"  : (i === 0) ? 0 : (i === 1) ?  0.127500 : 139.691700,
                            "city"       : (i === 0) ? "Local" : (i === 1) ? "London, England" : "Tokyo, Japan",
                            "timezone"   : 0
                          };
            localStorage.setItem("defaults" + i, JSON.stringify(tmpDefaults));
        } else {
            tmpDefaults = JSON.parse(defString);
        }
        defaults.push(tmpDefaults);
    }
    appData = {
                "fioKey"   : tmpFioKey,
                "defaults" : defaults 
              };
}

/*
 * Converts strings returned from forecast.io to values used on the watch
 */
function iconFromWeatherId(weatherId) {
    var conditions = {
      "unknown"            :  0,
      "clear-day"          :  1,
      "clear-night"        :  2,
      "rain"               :  3,
      "snow"               :  4,
      "sleet"              :  5,
      "wind"               :  6,
      "fog"                :  7,
      "cloudy"             :  8,
      "partly-cloudy-day"  :  9,
      "partly-cloudy-night": 10
    };
    if (conditions[weatherId]) {
        return conditions[weatherId];
    } else {
        console.log("iconFromWeatherID, bad weatherId: " + weatherId);
        return conditions.unknown;
    }
}

function sendQueueToPebble() {

    if (pbl_msg_queue.length > 0) {
        var nextMessage = pbl_msg_queue.shift();
        console.log("Sending msg: " + JSON.stringify(nextMessage));
        Pebble.sendAppMessage (
            nextMessage,
            function (e) {
                sendQueueToPebble();
            },
            function (e) {
                console.log("sendQueueToPebble, message failed to send: " + e.data.transactionId +
                            ", message: " + JSON.stringify(nextMessage));
                pbl_msg_queue.push(nextMessage);
            }
        );
    }
}

/*
 * Gets the weather information for a particular lat/long. Sends the return values to the watch.
 */
function fetchWeather(watch, latitude, longitude) {
    var response;
    var req = new XMLHttpRequest();
    console.log("fetchWeather.");
    req.open('GET', "http://api.forecast.io/forecast/" + appData.fioKey + "/" +
                    latitude + "," + longitude, true);
    req.onload = function (e) {
        console.log("fetchWeather req.status: " + req.status + ", watch: " + watch);
        if(req.status == 200) {
            response = JSON.parse(req.responseText);
            appData.defaults[watch].timezone = response.offset * 3600;
            localStorage.setItem("defaults" + watch,  JSON.stringify(appData.defaults[watch]));
            var sunrise_date  = new Date((response.daily.data[0].sunriseTime -
                                          appData.defaults[0].timezone +
                                          appData.defaults[watch].timezone) * 1000);
            var sunset_date   = new Date((response.daily.data[0].sunsetTime -
                                          appData.defaults[0].timezone +
                                          appData.defaults[watch].timezone) * 1000);
            var message;
            
            switch (watch) {
            case 0:
                message = {
                    "offset_w0"     : (response.offset * 3600),
                    "city_w0"       : appData.defaults[watch].city,
                    "background_w0" : +appData.defaults[watch].background,
                    "timedisp_w0"   : +appData.defaults[watch].timedisp,
                    "weather_w0"    : [ iconFromWeatherId(response.currently.icon),
                                        iconFromWeatherId(response.daily.data[1].icon),
                                        iconFromWeatherId(response.daily.data[2].icon),
                                        Math.round(response.currently.temperature),
                                        Math.round(response.daily.data[0].temperatureMax),
                                        Math.round(response.daily.data[1].temperatureMax),
                                        Math.round(response.daily.data[2].temperatureMax),
                                        Math.round(response.daily.data[0].temperatureMin),
                                        Math.round(response.daily.data[1].temperatureMin),
                                        Math.round(response.daily.data[2].temperatureMin),
                                        sunrise_date.getHours(),
                                        sunrise_date.getMinutes(),
                                        sunset_date.getHours(),
                                        sunset_date.getMinutes()                            ]};
                pbl_msg_queue.push(message);
                sendQueueToPebble();
                break;
            case 1:
                 message = {
                    "offset_w1"     : (response.offset * 3600),
                    "city_w1"       : appData.defaults[watch].city,
                    "background_w1" : +appData.defaults[watch].background,
                    "timedisp_w1"   : +appData.defaults[watch].timedisp,
                    "weather_w1"    : [ iconFromWeatherId(response.currently.icon),
                                        iconFromWeatherId(response.daily.data[1].icon),
                                        iconFromWeatherId(response.daily.data[2].icon),
                                        Math.round(response.currently.temperature),
                                        Math.round(response.daily.data[0].temperatureMax),
                                        Math.round(response.daily.data[1].temperatureMax),
                                        Math.round(response.daily.data[2].temperatureMax),
                                        Math.round(response.daily.data[0].temperatureMin),
                                        Math.round(response.daily.data[1].temperatureMin),
                                        Math.round(response.daily.data[2].temperatureMin),
                                        sunrise_date.getHours(),
                                        sunrise_date.getMinutes(),
                                        sunset_date.getHours(),
                                        sunset_date.getMinutes()                            ]};
                pbl_msg_queue.push(message);
                break;
            case 2:
                message = {
                    "offset_w2"     : (response.offset * 3600),
                    "city_w2"       : appData.defaults[watch].city,
                    "background_w2" : +appData.defaults[watch].background,
                    "timedisp_w2"   : +appData.defaults[watch].timedisp,
                    "weather_w2"    : [ iconFromWeatherId(response.currently.icon),
                                        iconFromWeatherId(response.daily.data[1].icon),
                                        iconFromWeatherId(response.daily.data[2].icon),
                                        Math.round(response.currently.temperature),
                                        Math.round(response.daily.data[0].temperatureMax),
                                        Math.round(response.daily.data[1].temperatureMax),
                                        Math.round(response.daily.data[2].temperatureMax),
                                        Math.round(response.daily.data[0].temperatureMin),
                                        Math.round(response.daily.data[1].temperatureMin),
                                        Math.round(response.daily.data[2].temperatureMin),
                                        sunrise_date.getHours(),
                                        sunrise_date.getMinutes(),
                                        sunset_date.getHours(),
                                        sunset_date.getMinutes()                            ]};
                pbl_msg_queue.push(message);
                sendQueueToPebble();
                break;
            default:
                message = "";
                break;
            }
            if (message !== "") {
            } else {
                console.log("fetchWeather bad watch: " + watch);
            }
        } else {                // if (req.status == 200)
            console.log("fetchWeather: Error, request status: " + req.status + ", lat: " +
                        latitude, ", long: " + longitude);
        }
    };
    req.send(null);
}

function cityFromGeocodeResults(results) {
    var cityState    = "";
    var locality     = "";
    var admin_area_1 = "";
    var country_ln   = "";
    for (var i = 0; i < results.length; i++) {
        for (var j=0; j < results[i].address_components.length; j++) {
            if (results[i].address_components[j].types[0] == "locality") {
                locality = results[i].address_components[j].long_name;
            } else if (results[i].address_components[j].types[0] == "administrative_area_level_1") {
                admin_area_1 = results[i].address_components[j].short_name;
            } else if (results[i].address_components[j].types[0] == "country") {
                country_ln = results[i].address_components[j].long_name;
            }
        }
        if (locality !== "" ) {
            break;
        }
    }
    if (cityState === "") {
        if (country_ln === "United States") {
            cityState = locality + ", " + admin_area_1;
        } else {
            cityState = locality + ", " + country_ln;
        }
    }
    return cityState;
}
        
function getCity(watch_num, latitude, longitude) {
    var response;
       var cityState = "";
    var req = new XMLHttpRequest();
    req.open('GET', "https://maps.googleapis.com/maps/api/geocode/json?latlng="+
                    latitude + "," + longitude + "&sensor=false", true);
    req.onload = function (e) {
        if(req.status == 200) {
            response = JSON.parse(req.responseText);
            if (response.status != "ZERO_RESULTS") {
                cityState = cityFromGeocodeResults(response.results);
            } else {
                cityState = "ZERO_RESULTS";
                console.log("Cannot reverse geocode, lat: " + latitude, + ", long: " + longitude);
            }
        } else {
            cityState = "Geocode Failed";
            console.log("getCity: reverse geocode failed: " + req.status);
        }
        appData.defaults[watch_num].city = cityState;
        localStorage.setItem("defaults" + watch_num,  JSON.stringify(appData.defaults[watch_num]));
        Pebble.sendAppMessage (
            {
               "city_w0":cityState,
            }
        );
        fetchWeather(watch_num, latitude, longitude);
    };
    req.send(null);
}

function locationSuccess(pos) {
    console.log("locationSuccess.");
    appData.defaults[0].latitude = pos.coords.latitude;
    appData.defaults[0].longitude = pos.coords.longitude;
    localStorage.setItem("defaults0", JSON.stringify(appData.defaults[0]));
    setTimeout(getCity(0, pos.coords.latitude, pos.coords.longitude), 1500); // Always watch 0 for location service
}

function locationError(err) {
    console.warn('Location error (' + err.code + '): ' + err.message);
    Pebble.sendAppMessage (
        {
            "city_w0":"Loc Unavailable",
        }
    );
}

Pebble.addEventListener("ready",
                        function(e) {
//                            console.log("ready event");
                            getDefaults();
//                            locationWatcher = navigator.geolocation.watchPosition(locationSuccess,
//                                                locationError, watchlocationOptions);
                            navigator.geolocation.getCurrentPosition(locationSuccess,
                                                locationError, getlocationOptions);
                            fetchWeather(1, appData.defaults[1].latitude, appData.defaults[1].longitude);
                            fetchWeather(2, appData.defaults[2].latitude, appData.defaults[2].longitude);
                        });

Pebble.addEventListener("appmessage",
                        function(e) {
//                            console.log("appmessage event: " + JSON.stringify(e.payload));
                            navigator.geolocation.getCurrentPosition(locationSuccess,
                                                locationError, getlocationOptions);
                            for (var i=1; i < 3; i++ ) {
                                fetchWeather(i, appData.defaults[i].latitude, appData.defaults[i].longitude);
                            }
                        });
                        
Pebble.addEventListener("showConfiguration",
                        function(e) {
//                            console.log("showConfiguration event");
                            var url = encodeURI("http://www.dkkrause.com/jwtconfig.html?appData=" +
                                                        JSON.stringify(appData));
                            console.log("showConfigurationEvent, URL: " + url);
                            Pebble.openURL(url);
                        });

Pebble.addEventListener("webviewclosed",
                        function(e) {
//                            console.log("webviewclosed event");
                            if (e.response) {       // Configuration information returned
//                                console.log("webviewclosed response: " + e.response);
                                appData = JSON.parse(e.response);
                                localStorage.setItem(fioKeyString, appData.fioKey);
                                for (var i = 0; i < 3; i++ ) {
                                    localStorage.setItem("defaults" + i,  JSON.stringify(appData.defaults[i]));
                                    getCity(i, appData.defaults[i].latitude, appData.defaults[i].longitude);
                                }
                            }                       // No configuration information returned (cancel)
                        });