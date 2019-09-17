//disconnect websocket when page reload 	
window.onbeforeunload = function(e) {
    websocket.onclose = function() {}; //
    websocket.close();
};


var websocket = null;

/** ID    
 *  0   { "app_id" : 0 } --> Request server send all data  , freq ,duty ,pwm onoff , led onoff   
 *      { "app_id" : 0 , "app_data" : [freq , duty , pwm onoff ,  led onoff]  } --> save as default 
 *                      
 *  1   { "app_id" : 1 } -->  Request server send Frequency
 *      { "app_id" : 1 , "app_data" : [freq]  } --> update freq 
 *  
 *  2   { "app_id" : 2 } -->  Request server send duty
 *      { "app_id" : 2 , "app_data" : [duty]  } --> update duty 
 *
 *  3   { "app_id" : 3 } -->  Request server send pwm
 *      { "app_id" : 3 , "app_data" : [pwm_on0ff]  } --> update pwm_onoff
 *  
 *  4   { "app_id" : 4 } -->  Request server send led
 *      { "app_id" : 4 , "app_data" : [led_on0ff]  } --> update led_onoff
 */

var app_id = {
    save: 0,
    freq: 1,
    duty: 2,
    pwm_onoff: 3,
    led_onoff: 4

};

var app_name = ["Save as dafault ",
    "Frequency setting ",
    "Duty cycle setting ",
    "PWM signal on/off control ",
    "Led on/off Control "
];

$(document).ready(function() {
    WebsocketInit();
    ShowMsg("Welcome to my signal generator APP!!", "red");
    ShowMsg("Please let me know if you have any questions.", "red");
});

var IsChanged = false;



function WebsocketInit() {
    //console.log(wsurl);
    //ws = new WebSocket(wsurl);
    // ws = new WebSocket(wsurl+'/ws');
    var url = (window.location.href).replace(/http/, "ws");

    console.log(url);
    websocket = new WebSocket(url + "ws");

    //Connection open event handler
    websocket.onopen = function(evt) {
            onOpen(evt);
        }
        //Connection error event handler
    websocket.onerror = function(evt) {
        onError(evt);
    }

    //if their socket closes unexpectedly, re-establish the connection
    websocket.onclose = function(evt) {
            onClose(evt);
        }
        //Event Handler to receive messages from server
    websocket.onmessage = function(evt) {
        onMessage(evt);
    }


}



function onOpen(evt) {
    $("#status").css("background-color", "#00ff00");
    $("#status").html("WebSocket Online");
    enable_all();


}

function onClose(evt) {
    $("#status").css("background-color", "#ff0000");
    $("#status").html("WebSocket Offline");
    ShowMsg("WebSocket close unexpectedly!", "red");
    disable_all();

    setTimeout(function() {
        WebsocketInit(); //reconnect websocket 
    }, 500);

}

function onMessage(evt) {

    console.log('Client - received socket message: ' + evt.data);
    var obj = JSON.parse(evt.data);
    //console.log(evt.data);
    if (!isEmpty(obj)) {

        switch (obj.app_id) {
            case 0: // { "app_id" : 0 , "app_data" : [freq , duty , pwm onoff ,  led onoff]  } --> default Value
                setInitial(obj.app_data);
                break;
            case 1: //freq
                setFreq(obj.app_data[0]);
                break;
            case 2: //duty 
                setDuty(obj.app_data[0]);
                break;
            case 3: //pwm on off 
                setPwm_onoff(obj.app_data[0]);
                break;
            case 4: // led on off 
                setLed_onoff(obj.app_data[0]);
                break;
			case 5: // Freq_step
              
                break;
			 case 6: // duty_step
               
                break;
            default:
                ShowMsg("Unknow id!", "red");
                break;
        }




    }

}

function setInitial(obj) {
    $("#slider-1").val((obj[0] / 1000).toFixed(1)).change();

    $("#slider-2").val(obj[1]).change();

    $("#btn-3").unbind("change");

    $("#btn-4").unbind("change");

    $("#btn-3").val(Number(obj[2])).flipswitch('refresh');
    $("#btn-4").val(Number(obj[3])).flipswitch('refresh');
    auto_freq_value = obj[0];
    auto_duty_value = obj[1];
    console.log("   auto_freq_value :" + auto_freq_value);
    console.log("   auto_duty_value :" + auto_duty_value);
    //PWM ON OFF Event
    $("#btn-3").bind("change", function(e) {

        onoff_control(app_id.pwm_onoff);

    });
    //LED ON OFF Event
    $("#btn-4").bind("change", function(e) {

        onoff_control(app_id.led_onoff);

    });
    //unchecked auto step freq duty .
    $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
    $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
    //clear timer
    clearInterval(freq_tmr);
    clearInterval(duty_tmr);
    //Save default Event
    UpdateFreqMeter((obj[0] / 1000).toFixed(1));
    UpdateDutyMeter(obj[1]);
    ShowMsg('Initial parameters completed.');
}


function setFreq(value) {
    console.log('freq value' + value);
    var old_freq = $("#slider-1").val();
    if ((value / 1000).toFixed(1) != old_freq) {
        $("#slider-1").val((value / 1000).toFixed(1)).change();
    }
    ShowMsg("Update " + (value / 1000).toFixed(1) + "k(Hz) Frequency finished!", "#033B74");
    UpdateFreqMeter((value / 1000).toFixed(1));
}

function setDuty(value) {

    var old_duty = $("#slider-2").val();
    console.log('old duty:' + old_duty);
    console.log('new duty:' + value);
    if (value != old_duty) {
        $("#slider-2").val(Number(value)).change();
    }
    ShowMsg("Update " + Number(value) + "(%) Duty finished!", "#033B74");
    UpdateDutyMeter(Number(value));

}

function setPwm_onoff(value) {
    var old_pwm_onoff = Number($('#btn-3').find(':selected').val());
    console.log('old_pwm_onoff:' + old_pwm_onoff);
    $("#btn-3").unbind("change");
    if (value != Boolean(old_pwm_onoff)) {
        $("#btn-3").val(Number(value)).flipswitch('refresh');
        ShowMsg("Update PWM output on/off state !", "#033B74");
    }

    $("#btn-3").bind("change", function(e) {

        onoff_control(app_id.pwm_onoff);

    });
}

function setLed_onoff(value) {
    var old_led_onoff = Number($('#btn-4').find(':selected').val());
    console.log('old_LED_onoff:' + old_led_onoff);
    $("#btn-4").unbind("change");
    if (value != Boolean(old_led_onoff)) {
        $("#btn-4").val(Number(value)).flipswitch('refresh');
        ShowMsg("Update LED PS on/off state.", "#033B74");
    }
    $("#btn-4").bind("change", function(e) {

        onoff_control(app_id.led_onoff);

    });
}



function onError(evt) {


}

//------check JSON String------ 
function IsJson(str) {
    "use strict";
    var tempobj;
    try {
        tempobj = JSON.parse(str);

    } catch (e) {
        return false;
    }

    return true;
}


function isEmpty(obj) {
    for (var key in obj) {
        if (obj.hasOwnProperty(key))
            return false;
    }
    return true;
}

function ShowMsg(msg, color) {
    // console.log(msg.data);
    var today = new Date();
    var t = today.getHours() + ':' + today.getMinutes() + ':' + today.getSeconds();
    if ($("#msg_board").children().length > 3) {

        $('#msg_board li:nth-child(1)').remove();
    }
    $("#msg_board").append('<li style="color : ' + color + ';font-size:14px">[ ' + t + ' ] &#9888' + msg + '</li>');

}

function UpdateFreqMeter(freq) {

    FreqGauge.value = freq;
}

function UpdateDutyMeter(duty) {
    DutyGauge.value = duty;
}

function Update(id) {
    var slider = "#slider-" + id;
    var data = [Number($('#slider-' + id).val())];
    if (!IsChanged) {


        if (websocket.readyState) {
            console.log('data[0]' + data[0]);
            if (id == app_id.freq) {

                app_write(id, [data[0] * 1000]);
            } else {
                app_write(id, [data[0]]);
            }

            ShowMsg(app_name[id] + "has been sent!", "#033B74");
        } else {

            ShowMsg("Websocket is not connect!", "red");
        }
        $(slider).slider('disable');
        setTimeout(function() {

            IsChanged = false;
            $(slider).slider('enable');
        }, 1000);


    }
    return 0;
}

function disable_all() {
    $('#slider-1').slider('disable');
    $('#slider-2').slider('disable');
    $('#btn-3').flipswitch('disable');
    $('#btn-4').flipswitch('disable');
    $('#Freq_step_chkbox').checkboxradio('disable');
    $('#Duty_step_chkbox').checkboxradio('disable');
    $('#btn-0').addClass('ui-disabled');

}

function enable_all() {

    $('#slider-1').slider('enable');
    $('#slider-2').slider('enable');
    $('#btn-3').flipswitch('enable');
    $('#btn-4').flipswitch('enable');
    $('#Freq_step_chkbox').checkboxradio('enable');
    $('#Duty_step_chkbox').checkboxradio('enable');
    $('#btn-0').removeClass('ui-disabled');
}

function onoff_control(id) {

    var onoff;
    var button = "#btn-" + id

    if (!IsChanged) {
        IsChanged = true;

        if ($(button).find(":selected").val() == 1) {
            onoff = true;


        } else {

            if (id == app_id.pwm_onoff) {

                $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
                $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
                clearInterval(freq_tmr);
                clearInterval(duty_tmr);
            }
            onoff = false;
            UpdateFreqMeter($("#slider-1").val());
            UpdateDutyMeter($("#slider-2").val());
        }


        if (websocket.readyState) {
            app_write(id, [onoff]);
            ShowMsg(app_name[id] + "has been sent!", "#033B74");
        } else {

            ShowMsg("Websocket is not connect!", "red");
        }
        $(button).flipswitch('disable');

        setTimeout(function() {

            IsChanged = false;
            $(button).flipswitch('enable');
        }, 1000);
    }
    return 0;
}

function app_save() {
    var freq = Number($("#slider-1").val()) * 1000;
    var duty = Number($("#slider-2").val());
    var pwm_onoff = Number($('#btn-3').find(':selected').val());
    var led_onoff = Number($('#btn-4').find(':selected').val());

    var obj = [freq, duty, Boolean(pwm_onoff), Boolean(led_onoff)];

    if (!IsChanged) {
        IsChanged = true;

        if (websocket.readyState) {
            app_write(app_id.save, obj);
            ShowMsg("Data has been sent!", "#033B74");
        } else {

            ShowMsg("Websocket is not connect!", "red");
        }

        $('#btn-0').addClass('ui-disabled');

        setTimeout(function() {
            $('#btn-0').removeClass('ui-disabled');
            IsChanged = false;

        }, 2000);
    }
    return 0;


}

function app_write(app_id, app_data) {
    var obj = {
        "app_id": app_id,
        "app_data": app_data
    };
    console.log('Send->>' + JSON.stringify(obj));
    if (websocket.readyState) {
        websocket.send(JSON.stringify(obj));
        return true;
    } else {
        ShowMsg("Can't send msg to Server!", '#FF0000');
        return false;
    }

}

function app_read(app_id) {
    var obj = {
        "app_id": app_id
    };
    ShowMsg(JSON.stringify(obj), "black");
    websocket.send(JSON.stringify(obj));
}
$("#div-1").on('slidestop', function() {

    Update(app_id.freq);

    //DutyGauge.value = ($('#duty_slider').val());
});

$("#div-2").on('slidestop', function() {

    Update(app_id.duty);

});

$("#btn-0").bind("click", function(e) {

    app_save();

});

var duty_start = 1, // 1% duty start
    duty_stop = 100, // 1% duty stop 
    duty_step = 2, //step 0.1% duty
    duty_time = 2000,
    duty_direction = true,
    auto_duty_value, duty_tmr; //500ms update

var freq_start = 10000, //start freq
    freq_stop = 30000, //stop freq
    freq_step = 500, //step 0.1kHz
    freq_time = 2000, //500ms update
    freq_direction = true,
    auto_freq_value, freq_tmr;

function auto_freq() {

    if (auto_freq_value > 30000) {
        auto_freq_value = 30000;
        freq_direction = false;
    }

    if (auto_freq_value < 10000) {
        auto_freq_value = 10000;
        freq_direction = true;
    }

    if (freq_direction) {
        auto_freq_value += freq_step;
    } else {
        auto_freq_value -= freq_step;
    }
    console.log('freq_Dir:' + freq_direction);
    console.log('auto_freq_value:' + auto_freq_value);

    return app_write(app_id.freq, [auto_freq_value]);
}

function auto_duty() {

    if (auto_duty_value > 100) {
        auto_duty_value = 100;
        duty_direction = false;
    }

    if (auto_duty_value < 1) {
        auto_duty_value = 1;
        duty_direction = true
    }

    if (duty_direction) {
        auto_duty_value += duty_step;
    } else {
        auto_duty_value -= duty_step;
    }
    console.log('Duty_Dir:' + duty_direction);
    console.log('auto_duty_value:' + auto_duty_value);
    return app_write(app_id.duty, [auto_duty_value]);


}
// auto freq events 
$("#Freq_step_chkbox").bind("change", function(e) {
    if (websocket.readyState) {
        if ($("#Freq_step_chkbox").is(":checked")) {

            if ($('#btn-3').find(":selected").val() == 1) {
                console.log('auto_freq_value:' + auto_freq_value);
                console.log("Freq Checked");
                //turn off auto duty claer timer 
                $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
                clearInterval(duty_tmr);
                //send freq repeat to server 
                freq_tmr = setInterval(function() {
                    if (auto_freq() == false) {

                        clearInterval(freq_tmr);
                    }
                }, freq_time);

            } else {
                $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
                // freq_direction = true;
                clearInterval(freq_tmr);
                ShowMsg("Please turn on PWM signal.", '#FF0000');
                // auto_freq_value = freq_start;
                //duty_direction = true;
                //clearInterval(duty_tmr);
                // auto_duty_value = duty_start;

            }


        } else {
            $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
            clearInterval(freq_tmr);
            ShowMsg("Stop freq step.", '#FF0000');

        }

    } else {
        $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
        ShowMsg("Please check websocket is online.", '#FF0000');
        clearInterval(freq_tmr);

    }
});



// auto duty events 
$("#Duty_step_chkbox").bind("change", function(e) {
    if (websocket.readyState) {
        if ($("#Duty_step_chkbox").is(":checked")) {

            if ($('#btn-3').find(":selected").val() == 1) {
                console.log('auto_duty_value:' + auto_duty_value);

                //turn off auto freq clear timer 
                $("#Freq_step_chkbox").prop("checked", false).checkboxradio("refresh");
                clearInterval(freq_tmr);
                //send Duty repeat to server 
                duty_tmr = setInterval(function() {
                    if (auto_duty() == false) {

                        clearInterval(duty_tmr);
                    }
                }, duty_time);

            } else {
                $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
                // freq_direction = true;
                clearInterval(duty_tmr);
                ShowMsg("Please turn on PWM signal.", '#FF0000');
                // auto_freq_value = freq_start;
                //duty_direction = true;
                //clearInterval(duty_tmr);
                // auto_duty_value = duty_start;

            }


        } else {
            $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
            clearInterval(duty_tmr);
            ShowMsg("Stop duty step.", '#FF0000');

        }

    } else {
        $("#Duty_step_chkbox").prop("checked", false).checkboxradio("refresh");
        ShowMsg("Please check websocket is online.", '#FF0000');
        clearInterval(duty_tmr);

    }
});