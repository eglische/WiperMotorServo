// Adapted version for IBT_2 H-bridge, fine tuned for a 250w wheelchair motor (75rpm) and a P3022-V1-CW360 rotary hall encoder.
// If you like to use a normal poti, just increase again "potMax" to 1024 for the full resolution.

#include <PID_v1.h>  // PID loop from http://playground.arduino.cc/Code/PIDLibrary

// Pin assignments
const int potPin = A0;
const int pwmInputPin = 2;
const int motorPin1 = 9;
const int motorPin2 = 10;
double deadband = 5;  // Increased deadband value to reduce micro-oscillations

// PID parameters (retuned for smoother control)
double Pk1 = 5;    // Reduced proportional gain for smoother correction
double Ik1 = 0.05; // Slightly increased integral gain for gradual corrections
double Dk1 = 0.5;  // Reduced derivative gain to reduce aggressive damping

// PID variables
double Setpoint1, Input1, Output1, Output1a;
PID PID1(&Input1, &Output1, &Setpoint1, Pk1, Ik1, Dk1, DIRECT);  // PID Setup

// PWM and timing variables
volatile unsigned long pwm;
volatile boolean done;
unsigned long start;
unsigned long currentMillis;
long previousMillis = 0;
const long interval = 50;  // Increased sample time for smoother corrections

// Potentiometer and PWM range settings (for fine-tuning)
int potMin = 0;
int potMax = 400;
int pwmMin = 1000;
int pwmMax = 2000;
int outputMin = -200;
int outputMax = 200;

// Filter variables
double alphaLowPass = 0.5;   // Low-pass filter smoothing factor
double alphaHighPass = 0.5;  // High-pass filter smoothing factor
double previousPWM = 1500;   // Initial guess for PWM (midpoint)
double filteredPWM = 1500;   // For the low-pass filter
double highPassPWM = 0;      // For the high-pass filter

// Declare pot variable
int pot;

void setup() {
  pinMode(pwmInputPin, INPUT);
  pinMode(potPin, INPUT);
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  
  // Set PWM frequency to 25 kHz for pins 9 and 10 (Timer 1)
  TCCR1B = TCCR1B & B11111000 | B00000001;  // Set the prescaler to 1 (for 25 kHz PWM)

  attachInterrupt(digitalPinToInterrupt(pwmInputPin), timeit, CHANGE);

  Serial.begin(115200);

  // PID Setup
  PID1.SetMode(AUTOMATIC);
  PID1.SetOutputLimits(outputMin, outputMax);
  PID1.SetSampleTime(interval);  // Increased sample time to 50 ms
}

void timeit() {
  if (digitalRead(pwmInputPin) == HIGH) {
    start = micros();
  } else {
    pwm = micros() - start;
    done = true;
  }
}

// Function to apply a low-pass filter
double lowPassFilter(double newPWM, double filteredPWM, double alpha) {
  return alpha * newPWM + (1 - alpha) * filteredPWM;
}

// Function to apply a high-pass filter
double highPassFilter(double newPWM, double previousPWM, double alpha) {
  return alpha * (highPassPWM + newPWM - previousPWM);
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {  // Start timed event
    previousMillis = currentMillis;

    // Read potentiometer value
    pot = analogRead(potPin);

    // Apply a low-pass filter on the PWM signal to smooth high-frequency noise
    filteredPWM = lowPassFilter(pwm, filteredPWM, alphaLowPass);

    // Apply a high-pass filter to remove low-frequency components (slow drift)
    highPassPWM = highPassFilter(pwm, previousPWM, alphaHighPass);
    previousPWM = pwm;  // Update previous PWM for high-pass filter

    // Print debug information
    Serial.print("Potentiometer: ");
    Serial.print(pot);
    Serial.print(" , Filtered PWM (Low-Pass): ");
    Serial.print(filteredPWM);
    Serial.print(" , High-Pass Filtered PWM: ");
    Serial.print(highPassPWM);
    Serial.print(" , Raw PWM: ");
    Serial.print(pwm);
    Serial.print(" , PID Output: ");
    Serial.println(Output1);

    // Map filtered PWM to output range
    Setpoint1 = map(filteredPWM, pwmMin, pwmMax, outputMin, outputMax);
    Input1 = map(pot, potMin, potMax, outputMin, outputMax);

    // Compute the PID output
    PID1.Compute();

    // Explicitly set small output values to zero if they are within the deadband
    if (abs(Output1) < deadband) {
      Output1 = 0;  // Treat small output as zero
    }

    // Deadband implementation to avoid micro-oscillations
    if (Output1 == 0) {
      // Stop the motor when the PID output is effectively zero
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, 0);
    } else if (Output1 > 0) {
      // Move forward if the output is positive
      analogWrite(motorPin1, Output1);
      analogWrite(motorPin2, 0);
    } else if (Output1 < 0) {
      // Move in reverse if the output is negative
      Output1a = abs(Output1);
      analogWrite(motorPin1, 0);
      analogWrite(motorPin2, Output1a);
    }

    if (!done)
      return;

    done = false;
  }  // End of timed event
}
