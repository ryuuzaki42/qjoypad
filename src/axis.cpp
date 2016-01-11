#include "axis.h"
#include "event.h"
#include "time.h"
#define sqr(a) ((a)*(a))
#define cub(a)	((a)*(a)*(a))
#define clamp(a, a_low, a_high)	\
	((a) < (a_low) ? (a_low) : (a) > (a_high) ? (a_high) : (a))


Axis::Axis( int i, QObject *parent ) : QObject(parent) {
    index = i;
    isOn = false;
    isDown = false;
    state = 0;
    gradient = false;
    toDefault();
    tick = 0;
    timer = new QTimer(this);
}


Axis::~Axis() {
    release();
    delete timer;
}

bool Axis::read( QTextStream* stream ) {
//	At this point, toDefault has just been called.

    //read in a line from the stream, and split it up into individual words
    QString input = stream->readLine().toLower();
    QRegExp regex("[\\s,]+");
    QStringList words = input.split(regex);

    //used to assure QString->int conversions worked
    bool ok;
    //int to store values derived from strings
    int val;
    float fval;
    //step through each word, check if it's a token we recognize
    for ( QStringList::Iterator it = words.begin(); it != words.end(); ++it ) {
        if (*it == "maxspeed") {
            ++it; //move to the next word, which should be the maximum speed.
            //if no value was given, there's an error in the file, stop reading.
            if (it == words.end()) return false;
            //try to convert the value.
            val = (*it).toInt(&ok);
            //if that worked and the maximum speed is in range, set it.
            if (ok && val >= 0 && val <= MAXMOUSESPEED) maxSpeed = val;
            //otherwise, faulty input, give up.
            else return false;
        }
        //pretty much the same process for getting the dead zone
        else if (*it == "dzone") {
            ++it;
            if (it == words.end()) return false;
            val = (*it).toInt(&ok);
            if (ok && val >= 0 && val <= JOYMAX) dZone = val;
            else return false;
        }
        //and again for the extreme zone,
        else if (*it == "xzone") {
            ++it;
            if (it == words.end()) return false;
            val = (*it).toInt(&ok);
            if (ok && val >= 0 && val <= JOYMAX) xZone = val;
            else return false;
        }
		else if (*it == "tcurve") {
			++it;
			if (it == words.end()) return false;
			val = (*it).toInt(&ok);
			if (ok && val >= 0 && val <= power_function) transferCurve = val;
			else return false;
		}
		else if (*it == "sens") {
			++it;
			if (it == words.end()) return false;
			fval = (*it).toFloat(&ok);
			if (ok && fval >= SENSITIVITY_MIN && fval <= SENSITIVITY_MAX)
				sensitivity = fval;
			else return false;
		}
        //and for the positive keycode,
        else if (*it == "+key") {
            ++it;
            if (it == words.end()) return false;
            val = (*it).toInt(&ok);
            if (ok && val >= 0 && val <= MAXKEY) pkeycode = val;
            else return false;
        }
        //and finally for the negative keycode.
        else if (*it == "-key") {
            ++it;
            if (it == words.end()) return false;
            val = (*it).toInt(&ok);
            if (ok && val >= 0 && val <= MAXKEY) nkeycode = val;
            else return false;
        }
        //the rest of the options are keywords without integers
        else if (*it == "gradient") {
            gradient = true;
        }
        else if (*it == "throttle+") {
            throttle = 1;
        }
        else if (*it == "throttle-") {
            throttle = -1;
        }
        else if (*it == "mouse+v") {
            mode = mousepv;
        }
        else if (*it == "mouse-v") {
            mode = mousenv;
        }
        else if (*it == "mouse+h") {
            mode = mouseph;
        }
        else if (*it == "mouse-h") {
            mode = mousenh;
        }
        //we ignore unrecognized words to be friendly and allow for additions to
        //the format in later versions. Note, this means that typos will not get
        //the desired effect OR produce an error message.
    }

    //assume that xZone, dZone, or maxSpeed has changed, for simplicity.
    //do a few floating point calculations.
    adjustGradient();

    //if we parsed through all of the words, yay! All done.
    return true;
}

void Axis::timerCalled() {
    timerTick(++tick);
}

void Axis::write( QTextStream* stream ) {
    *stream << "\t" << getName() << ": ";
    if (gradient)  *stream << "gradient, ";
    if (throttle > 0) *stream << "throttle+, ";
    else if (throttle < 0) *stream << "throttle-, ";
    if (dZone != DZONE) *stream << "dZone " << dZone << ", ";
    if (xZone != XZONE) *stream << "xZone " << xZone << ", ";
    if (mode == keybd) {
        *stream << "+key " << pkeycode << ", "
        << "-key " << nkeycode << "\n";
    }
    else {
        if (gradient) *stream << "maxSpeed " << maxSpeed << ", ";
		if (transferCurve != quadratic)
			*stream << "tCurve " << transferCurve << ", ";
		if (sensitivity != 1.0F)
			*stream << "sens " << sensitivity << ", ";
        *stream << "mouse";
        if (mode == mousepv)
            *stream << "+v\n";
        else if (mode == mousenv)
            *stream << "-v\n";
        else if (mode == mouseph)
            *stream << "+h\n";
        else if (mode == mousenh)
            *stream << "-h\n";
    }

}

void Axis::release() {
    //if we're pressing a key, let it go.
    if (isDown) {
        move(false);
        isDown = false;
    }
}

void Axis::jsevent( int value ) {
    //adjust real value to throttle value
    if (throttle == 0)
        state = value;
    else if (throttle == -1)
        state = (value + JOYMIN) / 2;
    else
        state = (value + JOYMAX) / 2;
    //set isOn, deal with state changing.
    //if was on but now should be off:
    if (isOn && abs(state) <= dZone) {
        isOn = false;
        if (gradient) {
            duration = 0;
            release();
            timer->stop();
            disconnect(timer, SIGNAL(timeout()), 0, 0);
            tick = 0;
        }
    }
    //if was off but now should be on:
    else if (!isOn && abs(state) >= dZone) {
        isOn = true;
        if (gradient) {
            duration = (abs(state) * FREQ) / JOYMAX;
            connect(timer, SIGNAL(timeout()), this, SLOT(timerCalled()));
            timer->start(MSEC);
        }
    }
    //otherwise, state doesn't change! Don't touch it.
    else return;

    //gradient will trigger movement on its own via timer().
    //non-gradient needs to be told to move.
    if (!gradient) {
        move(isOn);
    }
}

void Axis::toDefault() {
    release();
    gradient = false;
    throttle = 0;
    maxSpeed = 100;
	transferCurve = quadratic;
	sensitivity = 1.0F;
    dZone = DZONE;
    tick = 0;
    xZone = XZONE;
    mode = keybd;
    pkeycode = 0;
    nkeycode = 0;
    downkey = 0;
    state = 0;
    adjustGradient();
}

bool Axis::isDefault() {
    return (gradient == false) &&
           (throttle == 0) &&
           (maxSpeed == 100) &&
           (dZone == DZONE) &&
           (xZone == XZONE) &&
           (mode == keybd) &&
           (pkeycode == 0) &&
           (nkeycode == 0);
}

bool Axis::inDeadZone( int val ) {
    int value;
    if (throttle == 0)
        value = val;
    else if (throttle == -1)
        value = (val + JOYMIN) / 2;
    else
        value = (val + JOYMAX) / 2;
    return (abs(value) < dZone);
}

QString Axis::status() {
    QString result = getName() + " : [";
    if (mode == keybd) {
        if (throttle == 0)
            result += "KEYBOARD";
        else result += "THROTTLE";
    }
    else result += "MOUSE";
    return result + "]";
}

void Axis::setKey(bool positive, int value) {
    if (positive)
        pkeycode = value;
    else
        nkeycode = value;
}

void Axis::timerTick( int tick ) {
    if (isOn) {
        if (mode == keybd) {
            if (tick % FREQ == 0)
            {
                if (duration == FREQ)
                {
                    if (!isDown) move(true);
                    duration = (abs(state) * FREQ) / JOYMAX;
                    return;
                }
                move(true);
            }
            if (tick % FREQ == duration) {
                move(false);
                duration = (abs(state) * FREQ) / JOYMAX;
            }
        }
        else {
            move(true);
        }
    }
}

void Axis::adjustGradient() {
	inverseRange = 1.0F / (xZone - dZone);
	// This is also the convenient spot to initialize the dithering
	// accmulator.
    sumDist = 0;
}

void Axis::move( bool press ) {
    xevent e;
    if (mode == keybd) {
        //prevent KeyPress-KeyPress and KeyRelease-KeyRelease pairs.
        //this would only happen in odd circumstances involving the setup
        //dialog being open and blocking events from happening.
        if (isDown == press) return;
        isDown = press;
        if (press) {
            e.type = KPRESS;
            downkey = (state > 0)?pkeycode:nkeycode;
        }
        else {
            e.type = KREL;
        }
        e.value1 = downkey;
        e.value2 = 0;
    }
    //if using the mouse
    else if (press) {
        int dist;

 		if (gradient) {
			const int absState = abs(state);
			float fdist;	// Floating point movement distance

			if (absState >= xZone) fdist = 1.0F;
			else if (absState <= dZone) fdist = 0.0F;
			else {
				const float u = inverseRange * (absState - dZone);

				switch(transferCurve) {
				case quadratic:
					fdist = sqr(u);
					break;
				case cubic:
					fdist = cub(u);
					break;
				case quadratic_extreme:
					fdist = sqr(u);
					if(u >= 0.95F) {
						fdist *= 1.5F;
					}
					break;
				case power_function:
					fdist = clamp(powf(u, 1.0F / clamp(
						sensitivity, 1e-8F, 1e+3F)), 0.0F, 1.0F);
					break;
				default:
					fdist = u;
				}
			}
			fdist *= maxSpeed;
			if (state < 0) fdist = -fdist;
			// Accumulate the floating point distance and shift the
			// mouse by the rounded magnitude
			sumDist += fdist;
			dist = static_cast<int>(rint(sumDist));
            sumDist -= dist;
 		}
 		//if not gradient, always go full speed.
 		else dist = maxSpeed;
 
 		e.type = WARP;
 		if (mode == mousepv) {
 			e.value1 = 0;
            e.value2 = dist;
        }
        else if (mode == mousenv) {
            e.value1 = 0;
            e.value2 = -dist;
        }
        else if (mode == mouseph) {
            e.value1 = dist;
            e.value2 = 0;
        }
        else if (mode == mousenh) {
            e.value1 = -dist;
            e.value2 = 0;
        }
    }
    //actually create the event
    sendevent(e);
}
