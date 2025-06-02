/********************************************************************
 *  Ratón-Teclado híbrido · PMW3360 + Joystick + Matriz 3×4
 *  ► Modo WASD (palanca) + Espacio (botón del joystick)
 ********************************************************************/

#include <SPI.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <avr/pgmspace.h>
#include "firmware_data.h"

/* ───── CONFIGURACIÓN GENERAL ──────────────────────────────────── */

// Umbral de zona muerta para el joystick (evita entradas no deseadas por ruido)
const int DEADZONE = 50;

// Sensibilidad de movimiento del ratón (modificable para ajustar velocidad)
float mouseSensitivityX = 0.8;
float mouseSensitivityY = 0.8;

/* ───── DEFINICIÓN DE PINES ────────────────────────────────────── */

const int PIN_SENSOR_NCS     = 10;   // CS del PMW3360
const int PIN_JOYSTICK_X     = A0;   // Eje X del joystick
const int PIN_JOYSTICK_Y     = A1;   // Eje Y del joystick
const int PIN_JOYSTICK_BTN   = A2;   // Botón del joystick
const int PIN_LED_INDICADOR  = 17;   // LED de estado

const int ROW_PINS[3] = {7, 8, 9};   // Pines de filas para la matriz 3x4
const int COL_PINS[4] = {2, 4, 5, 6};// Pines de columnas para la matriz 3x4

/* ───── MAPA DE TECLAS/MOUSE EN MATRIZ ─────────────────────────── */

// Códigos especiales para botones de mouse
#define MOUSE_BTN_LEFT   1
#define MOUSE_BTN_RIGHT  2

const uint16_t keyMap[3][4] = 
{
  { MOUSE_BTN_LEFT,  MOUSE_BTN_RIGHT, 'r',            ' '            },
  { 'q',             'f',             'g',            KEY_LEFT_SHIFT },
  { 'z',             'x',             'c',            KEY_LEFT_CTRL  }
};

/* ───── VARIABLES DE ESTADO ────────────────────────────────────── */

bool wasPressed_W = false, wasPressed_A = false, wasPressed_S = false, wasPressed_D = false;
bool previousMatrixState[3][4];     // Guarda el estado anterior de cada botón en la matriz
bool previousJoystickButton = false; // Guarda el estado anterior del botón del joystick

/* ───── FUNCIONES AUXILIARES PARA EL SENSOR PMW3360 ───────────── */

void adns_com_begin()
{
  digitalWrite(PIN_SENSOR_NCS, LOW);
}

void adns_com_end()
{
  digitalWrite(PIN_SENSOR_NCS, HIGH);
}

byte adns_read_reg(byte reg)
{
  adns_com_begin();
  SPI.transfer(reg & 0x7F);
  delayMicroseconds(100);
  byte data = SPI.transfer(0);
  delayMicroseconds(1);
  adns_com_end();
  delayMicroseconds(19);
  return data;
}

void adns_write_reg(byte reg, byte value)
{
  adns_com_begin();
  SPI.transfer(reg | 0x80);
  SPI.transfer(value);
  delayMicroseconds(20);
  adns_com_end();
  delayMicroseconds(100);
}

void adns_upload_firmware()
{
  adns_write_reg(0x10, 0x20);
  adns_write_reg(0x13, 0x1D);
  delay(10);
  adns_write_reg(0x13, 0x18);

  adns_com_begin();
  SPI.transfer(0x62 | 0x80);
  delayMicroseconds(15);

  for (unsigned short i = 0; i < firmware_length; i++)
  {
    SPI.transfer(pgm_read_byte(firmware_data + i));
    delayMicroseconds(15);
  }

  adns_com_end();

  adns_read_reg(0x2A);
  adns_write_reg(0x10, 0x00);
  adns_write_reg(0x0F, 0x15);
}

void adns_initialize()
{
  adns_com_end();
  adns_com_begin();
  adns_com_end();

  adns_write_reg(0x3A, 0x5A);
  delay(50);

  // Leer registros para iniciar el sensor
  adns_read_reg(0x02);
  adns_read_reg(0x03);
  adns_read_reg(0x04);
  adns_read_reg(0x05);
  adns_read_reg(0x06);

  adns_upload_firmware();
  delay(10);
}

void readSensorMotion(int16_t &dx, int16_t &dy)
{
  adns_write_reg(0x02, 0x01); // Trigger motion burst
  adns_read_reg(0x02);        // Dummy read

  byte dxL = adns_read_reg(0x03);
  byte dxH = adns_read_reg(0x04);
  byte dyL = adns_read_reg(0x05);
  byte dyH = adns_read_reg(0x06);

  dx = (int16_t)(((uint16_t)dxH << 8) | dxL);
  dy = (int16_t)(((uint16_t)dyH << 8) | dyL);
}

/* ───── SETUP ──────────────────────────────────────────────────── */

void setup()
{
  Keyboard.begin();
  Mouse.begin();
  Serial.begin(115200);

  pinMode(PIN_SENSOR_NCS, OUTPUT);
  digitalWrite(PIN_SENSOR_NCS, HIGH);

  SPI.begin();
  SPI.setDataMode(SPI_MODE3);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV64);

  adns_initialize();

  pinMode(PIN_JOYSTICK_BTN, INPUT_PULLUP);
  pinMode(PIN_LED_INDICADOR, OUTPUT);
  digitalWrite(PIN_LED_INDICADOR, LOW);

  // Inicialización de pines de la matriz 3x4
  for (int r = 0; r < 3; r++)
  {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }

  for (int c = 0; c < 4; c++)
  {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }

  // Inicializar estados anteriores de los botones
  for (int r = 0; r < 3; r++)
  {
    for (int c = 0; c < 4; c++)
    {
      previousMatrixState[r][c] = false;
    }
  }
}

/* ───── LOOP PRINCIPAL ─────────────────────────────────────────── */

void loop()
{
  /* 1. Movimiento del ratón con sensor óptico -------------------- */
  int16_t deltaX = 0, deltaY = 0;
  readSensorMotion(deltaX, deltaY);

  signed char moveX = constrain((int)(deltaX * mouseSensitivityX), -127, 127);
  signed char moveY = constrain((int)(deltaY * mouseSensitivityY), -127, 127);

  if (moveX || moveY)
  {
    Mouse.move(moveX, moveY);
  }

  /* 2. Botón del joystick para salto (espacio) ------------------- */
  bool currentJoystickButton = (digitalRead(PIN_JOYSTICK_BTN) == LOW);

  if (currentJoystickButton && !previousJoystickButton)
  {
    Keyboard.press(' ');
  }
  else if (!currentJoystickButton && previousJoystickButton)
  {
    Keyboard.release(' ');
  }

  previousJoystickButton = currentJoystickButton;

  /* 3. Movimiento tipo WASD con el joystick ---------------------- */
  int joyX = analogRead(PIN_JOYSTICK_X) - 512;
  int joyY = analogRead(PIN_JOYSTICK_Y) - 512;

  bool pressW = false, pressA = false, pressS = false, pressD = false;

  if (joyY >  DEADZONE) pressD = true; // Derecha
  if (joyY < -DEADZONE) pressA = true; // Izquierda
  if (joyX >  DEADZONE) pressW = true; // Adelante
  if (joyX < -DEADZONE) pressS = true; // Atrás

  if (pressW && !wasPressed_W)       Keyboard.press('w'); else if (!pressW && wasPressed_W)       Keyboard.release('w');
  if (pressA && !wasPressed_A)       Keyboard.press('a'); else if (!pressA && wasPressed_A)       Keyboard.release('a');
  if (pressS && !wasPressed_S)       Keyboard.press('s'); else if (!pressS && wasPressed_S)       Keyboard.release('s');
  if (pressD && !wasPressed_D)       Keyboard.press('d'); else if (!pressD && wasPressed_D)       Keyboard.release('d');

  wasPressed_W = pressW;
  wasPressed_A = pressA;
  wasPressed_S = pressS;
  wasPressed_D = pressD;

  /* 4. Lectura de matriz de botones ------------------------------ */
  for (int r = 0; r < 3; r++)
  {
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(5);

    for (int c = 0; c < 4; c++)
    {
      bool pressed = (digitalRead(COL_PINS[c]) == LOW);

      uint16_t code = keyMap[r][c];

      if (pressed && !previousMatrixState[r][c])
      {
        if (code == MOUSE_BTN_LEFT)        Mouse.press(MOUSE_LEFT);
        else if (code == MOUSE_BTN_RIGHT)  Mouse.press(MOUSE_RIGHT);
        else                               Keyboard.press((uint8_t)code);
      }
      else if (!pressed && previousMatrixState[r][c])
      {
        if (code == MOUSE_BTN_LEFT)        Mouse.release(MOUSE_LEFT);
        else if (code == MOUSE_BTN_RIGHT)  Mouse.release(MOUSE_RIGHT);
        else                               Keyboard.release((uint8_t)code);
      }

      previousMatrixState[r][c] = pressed;
    }

    digitalWrite(ROW_PINS[r], HIGH);
  }

  delay(1); // Pequeña pausa para evitar rebotes
}
