/*global require, __dirname, console*/
var express = require('express'),
    net = require('net'),
    N = require('./nuve'),
    fs = require("fs"),
    https = require("https"),
    config = require('./../../lynckia_config');

var options = {
    key: fs.readFileSync('cert/key.pem').toString(),
    cert: fs.readFileSync('cert/cert.pem').toString()
};

var app = express();

app.use(express.bodyParser());

app.configure(function () {
    "use strict";
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));
    //app.set('views', __dirname + '/../views/');
    //disable layout
    //app.set("view options", {layout: false});
});

app.use(function (req, res, next) {
    "use strict";
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
    res.header('Access-Control-Allow-Headers', 'origin, content-type');
    if (req.method == 'OPTIONS') {
        res.send(200);
    }
    else {
        next();
    }
});

N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:13000/');

app.post('/createToken/:room', function (req, res) {
    "use strict";
        var room = req.params.room,
        username = req.body.username,
        role = req.body.role;
    N.API.createToken(room, username, role, function (token) {
        console.log(token);
        res.send(token);
    });
});

app.get('/getRooms/', function (req, res) {
    "use strict";
    N.API.getRooms(function (rooms) {
        res.send(rooms);
    });
});

app.get('/getRoomByName/:room', function (req, res) {
    "use strict";
        var room = req.params.room;
        var roomID;
    N.API.getRooms(function (roomList) {
                var rooms = JSON.parse(roomList);
                console.log("Get Rooms number",rooms.length);
                for(var i in rooms) {
                        console.log("Analyzing ",i,"room - with name",rooms[i].name);
                        if (rooms[i].name==room) {
                                roomID = rooms[i]._id;
                                console.log('Founded room',roomID);
                                console.log('And Name', rooms[i].name);
                                res.send(roomID);
                        }
                }
                console.log('resulting RoomID ', roomID);
                if (typeof roomID==='undefined') {
                        console.log('Room Not Found - Create New One');
                        N.API.createRoom(room, function (newRoomID) {
                            roomID = newRoomID._id;
                            console.log('Created room with ID ', roomID);
                            console.log('And name ', newRoomID.name);
                            res.send(roomID);
                    });
            }
});
});

app.get('/getUsers/:room', function (req, res) {
"use strict";
var room = req.params.room;
N.API.getUsers(room, function (users) {
    res.send(users);
});
});

app.listen(12001);
var server = https.createServer(options, app);
server.listen(12004);
