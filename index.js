var Fs, Events, eegdriver, EEGDriver;
Fs = require('fs');
Events = require('events');
eegdriver = require('./build/default/eegdriver');
exports.EEGDriver = EEGDriver = (function(superclass){
  EEGDriver.displayName = 'EEGDriver';
  var prototype = __extends(EEGDriver, superclass).prototype, constructor = EEGDriver;
  prototype.usb_fd = 0;
  function EEGDriver(device){
    var driver, usb, _this = this;
    driver = this.driver = new eegdriver.EEGDriver();
    usb = Fs.createReadStream(device, {
      flags: 'w+'
    });
    usb.on('open', function(fd){
      var cmd;
      console.log("opened usb", fd);
      this.usb_fd = fd;
      cmd = new Buffer("eeg\n");
      return Fs.write(fd, cmd, 0, cmd.length, null, function(err, written, buffer){
        var header, cmd;
        if (err) {
          throw err;
        }
        console.log("mode eeg", written);
        header = driver.header();
        cmd = new Buffer(header.length + 1 + "setheader ".length);
        cmd.write("setheader ");
        header.copy(cmd, "setheader ".length);
        cmd[cmd.length - 1] = "\n";
        return Fs.write(fd, cmd, 0, cmd.length, null, function(err, written, buffer){
          if (err) {
            throw err;
          }
          return console.log("set header", written);
        });
      });
    });
    usb.on('data', function(data){
      var i, x, _len, _results = [];
      for (i = 0, _len = data.length; i < _len; ++i) {
        x = data[i];
        _results.push(driver.gobble(x));
      }
      return _results;
    });
    driver.on('syncError', function(){
      return console.log('sync error');
    });
    driver.on('setHeader', function(header){
      return console.log('setHeader', header);
    });
    driver.on('data', function(data){
      return _this.emit("data", data);
    });
  }
  return EEGDriver;
}(Events.EventEmitter));
function __extends(sub, sup){
  function ctor(){} ctor.prototype = (sub.superclass = sup).prototype;
  (sub.prototype = new ctor).constructor = sub;
  if (typeof sup.extended == 'function') sup.extended(sub);
  return sub;
}