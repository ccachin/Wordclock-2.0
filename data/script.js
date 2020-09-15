
window.addEventListener('load', setup);



/*------------------------------------------------------------------------------------ Brightness management */
function updateBrightSlider(brightSlideAmount) {
    brightSlideVal = brightSlideAmount;
    console.log(brightSlideVal);
    $.post("setBrightness", {
        brightnessVal: brightSlideVal
    });

}
function updateFrontSlider(FrontSlideAmount) {
    frontSlideVal = FrontSlideAmount;
    console.log(frontSlideVal);
    $.post("setFrontBrightness", {
        frontBrightnessVal: frontSlideVal
    });

}

function updateBackSlider(BackSlideAmount) {
    backSlideVal = BackSlideAmount;
    console.log(backSlideVal);
    $.post("setBackBrightness", {
        backBrightnessVal: backSlideVal
    });

}
/*----------------------------------------------------------------------------------- Submit selected values  */
function submitVal(side, colorPicked, front, right, left, bottom, top) {
    sideSelected = side;
    colorVal = colorPicked;
    frontColorVal = front;
    backRightColorVal = right;
    backLeftColorVal = left;
    backBottomColorVal = bottom;
    backTopColorVal = top;

    console.log(colorVal);
    colorVal = colorVal.replace("#", "0x");

    if (sideSelected == "front") {
        $.post("setColor", {
            frontcolor: colorVal
        });
    }
    else if (sideSelected == "back_left") {
        $.post("setColor", {
            backLeftColor: colorVal
        });
    }
    else if (sideSelected == "back_right") {
        $.post("setColor", {
            backRightColor: colorVal
        });
    }
    else if (sideSelected == "back_top") {
        $.post("setColor", {
            backTopColor: colorVal
        });
    }
    else if (sideSelected == "back_bottom") {
        $.post("setColor", {
            backBottomColor: colorVal
        });
    }
    else if (sideSelected == "preset") {
        $.post("setColor", {
            frontcolor: frontColorVal,
            backRightColor: backRightColorVal,
            backLeftColor: backLeftColorVal,
            backBottomColor: backBottomColorVal,
            backtopColor: backTopColorVal
        });
    }

}

/*----------------------------------------------------------------------------------- Manage presets colors  */
function colorPreset(preset) {
    console.log(preset);
    switch (preset) {
        case 1: //Spring
            frontColorVal = 0x3EFF00;
            backRightcolorVal = 0xD7FF00;
            backLeftcolorVal = 0xD7FF00;
            backBottomcolorVal = 0x00FF17;
            backTopcolorVal = 0xF0FF00;
            break;
        case 2: //Summer
            frontColorVal = 0xFFC91F;
            backRightcolorVal = 0xFFFFFF;
            backLeftcolorVal = 0xFFFFFF;
            backBottomcolorVal = 0xFF9003;
            backTopcolorVal = 0x00F0FF;
            break;
        case 3: //Autumn
            frontColorVal = 0x3EFF00;
            backRightcolorVal = 0xD7FF00;
            backLeftcolorVal = 0xD7FF00;
            backBottomcolorVal = 0xFFFFFF;
            backTopcolorVal = 0xFFFFFF;
            break;
        case 4: //Winter
            frontColorVal = 0x3EFF00;
            backRightcolorVal = 0xD7FF00;
            backLeftcolorVal = 0xD7FF00;
            backBottomcolorVal = 0xD7FF00
            backTopcolorVal = 0xD7FF00
            break;
        default: //Unknown
            console.log("unknown selection");
    }
    submitVal('preset', '#FFFFFF', frontColorVal, backRightcolorVal, backLeftcolorVal, backBottomcolorVal, backTopcolorVal);
}



/*----------------------------------------------------------------------------------- Manage color pickers  */
function update(jscolor) {
    document.getElementById("ValeurColTexte").value = jscolor;
    document.getElementById('ValeurColFondGauche').value = jscolor;
    document.getElementById('ValeurColFondDroit').value = jscolor;
    document.getElementById('ValeurColFondHaut').value = jscolor;
    document.getElementById('ValeurColFondBas').value = jscolor;
}

function mouseUpFront() {
    if (document.getElementById('boutonTexte').value == "Valider") {
        document.getElementById('boutonTexte').value = "Texte";
        document.getElementById('boutonTexte').jscolor.hide();
        submitVal('front', document.getElementById("ValeurColTexte").value);
    }
    else {
        document.getElementById('boutonTexte').value = "Valider";
    }
}
function mouseUpBackLeft() {
    if (document.getElementById('boutonFondGauche').value == "Valider") {
        document.getElementById('boutonFondGauche').value = "Gauche";
        document.getElementById('boutonFondGauche').jscolor.hide();
        submitVal('back_left', document.getElementById('ValeurColFondGauche').value);
    }
    else {
        document.getElementById('boutonFondGauche').value = "Valider";
    }
}

function mouseUpBackRight() {
    if (document.getElementById('boutonFondDroit').value == "Valider") {
        document.getElementById('boutonFondDroit').value = "Droite";
        document.getElementById('boutonFondDroit').jscolor.hide();
        submitVal('back_right', document.getElementById('ValeurColFondDroit').value);
    }
    else {
        document.getElementById('boutonFondDroit').value = "Valider";
    }
}

function mouseUpBackTop() {
    if (document.getElementById('boutonFondHaut').value == "Valider") {
        document.getElementById('boutonFondHaut').value = "Haut";
        document.getElementById('boutonFondHaut').jscolor.hide();
        submitVal('back_top', document.getElementById('ValeurColFondHaut').value);
    }
    else {
        document.getElementById('boutonFondHaut').value = "Valider";
    }
}

function mouseUpBackBottom() {
    if (document.getElementById('boutonFondBas').value == "Valider") {
        document.getElementById('boutonFondBas').value = "Bas";
        document.getElementById('boutonFondBas').jscolor.hide();
        submitVal('back_bottom', document.getElementById('ValeurColFondBas').value);
    }
    else {
        document.getElementById('boutonFondBas').value = "Valider";
    }
}

/*----------------------------------------------------------------------------------- Set alarm */
function setStandBy() {
    frontstandByBrightness = document.getElementById('frontStandByslide').value;
    backstandByBrightness = document.getElementById('backStandByslide').value;
    startTimeVal = document.getElementById('startTime').value;
    endTimeVal = document.getElementById('endTime').value;

    startTimeArray = startTimeVal.split(":");
    endTimeArray = endTimeVal.split(":");

    startHours = startTimeArray[0];
    startMinutes = startTimeArray[1];
    endHours = endTimeArray[0];
    endMinutes = endTimeArray[1];

    var validation;

    if (startHours == "") {
        alert("Veuillez saisir une heure de début");
        startValidation = false;
    }
    else { startValidation = true; }
    if (endHours == "") {
        alert("Veuillez saisir une heure de fin");
        endValidation = false;
    }
    else { endValidation = true; }
    if (startValidation == true && endValidation == true) {
        alert("Heures de mise en veille sauvegardées.\nDébut: " + startHours + ":" + startMinutes + " \nFin: " + endHours + ":" + endMinutes + "\nN'oubliez pas de l'activer.");
        $.post("setStandBy", {
            startHoursSet: startHours,
            startMinuteSet: startMinutes,
            endHoursSet: endHours,
            endMinutesSet: endMinutes,
            frontBrightnessSet: frontstandByBrightness,
            backBrightnessSet: backstandByBrightness
        });
    }
}

function activateStandBy() {
    var isChecked = document.getElementById("activate").checked;
    if (isChecked) {
        standBy = 1;
        $.post("activateStandBy", {
            standByState: standBy
        });
    }
    else {
        standBy = 0;
        $.post("activateStandBy", {
            standByState: standBy
        });
    }

}


