#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define INITSCALE (int32_t)(SCREENHEIGHT * PI / 2)
#define INITSPEED 0.0002f
#define SCALESPEED 1.01f
#define MOVESPEED (1<<14)
#define ANIMSPEEDCHANGE 0.00001f

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256
#define SCREENWIDTH 128
#define SCREENHEIGHT 128
#define PI 3.1415926535897932384626433832795
#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1 << SINTABLEPOWER)
#define ANG1INC (int32_t)((CURVESTEP * SINTABLEENTRIES) / 235)
#define ANG2INC (int32_t)((CURVESTEP * SINTABLEENTRIES) / (2 * PI))

#define BTN_LEFT     0
#define BTN_RIGHT    2
#define BTN_UP       1
#define BTN_DOWN     3
#define BTN_A       21
#define BTN_B       25
#define BTN_LB       6
#define BTN_RB      22
#define BTN_MENU    26

#define TFT_X_OFFSET 2
#define TFT_Y_OFFSET 1

#define TFT_MOSI    19
#define TFT_SCLK    18
#define TFT_CS      17
#define TFT_DC      16
#define TFT_RST      4
#define TFT_BL       7

class ThumbyDisplay : public Adafruit_ST7735
{
public:
    ThumbyDisplay(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) {}

    void begin()
    {
        initR(INITR_144GREENTAB);

        _xstart = TFT_X_OFFSET;
        _ystart = TFT_Y_OFFSET;

        invertDisplay(true);
        fillScreen(ST77XX_BLACK);

        setSPISpeed(40000000);
    }
};

ThumbyDisplay tft = ThumbyDisplay(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(128, 128);
uint16_t* buffer = canvas.getBuffer();
uint16_t* screenCentre = buffer + ((SCREENWIDTH + SCREENWIDTH * SCREENHEIGHT) >> 1);

int32_t xOffset = 0.0f;
int32_t yOffset = 0.0f;
float speed = INITSPEED;
float scale = INITSCALE;
float animationTime = 0.0f;
float oldSpeed = 0.0f;
bool pausePressed = false;
uint32_t oldTime = 0;
int32_t sinTable[SINTABLEENTRIES];


void makeSinTable()
{
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        sinTable[i] = sin(i * 2 * PI / SINTABLEENTRIES) * SINTABLEENTRIES / (2 * PI);
    }
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        *((int16_t *)(sinTable + i) + 1) = *(int16_t *)(sinTable + ((i + SINTABLEENTRIES / 4) % SINTABLEENTRIES));
    }
}

void setup()
{
    SPI.setSCK(TFT_SCLK);
    SPI.setTX(TFT_MOSI);
    SPI.begin();

    tft.begin();

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    const int buttons[] = { BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_A, BTN_B, BTN_LB, BTN_RB, BTN_MENU };
    for (int pin : buttons) { pinMode(pin, INPUT_PULLUP); }

    makeSinTable();
    oldTime = micros();
}

void controls()
{
    if (digitalRead(BTN_LEFT) == LOW) { xOffset += MOVESPEED; }
    if (digitalRead(BTN_RIGHT) == LOW) { xOffset -= MOVESPEED; }
    if (digitalRead(BTN_UP) == LOW) { yOffset += MOVESPEED; }
    if (digitalRead(BTN_DOWN) == LOW) { yOffset -= MOVESPEED; } 
    if (digitalRead(BTN_A) == LOW && digitalRead(BTN_MENU) != LOW) { scale *= SCALESPEED; xOffset *= SCALESPEED; yOffset *= SCALESPEED; }
    if (digitalRead(BTN_B) == LOW) { scale /= SCALESPEED; xOffset /= SCALESPEED; yOffset /= SCALESPEED; }
    if (digitalRead(BTN_LB) == LOW) { speed += ANIMSPEEDCHANGE; }
    if (digitalRead(BTN_RB) == LOW) { speed -= ANIMSPEEDCHANGE; }
    if (digitalRead(BTN_MENU) == LOW)
    {
        if (!pausePressed)
        {
            pausePressed = true;
            if (speed != 0.0f)
            {
                oldSpeed = speed;
                speed = 0.0f;
            }
            else
            {
                speed = oldSpeed;
            }
        }
        if (digitalRead(BTN_A) == LOW)
        {
            scale = INITSCALE;
            xOffset = 0;
            yOffset = 0;
            speed = INITSPEED;
        }
    }
    else
    {
        pausePressed = false;
    }
}

void loop()
{
    uint32_t newTime = micros();
    uint32_t deltaTime = newTime - oldTime;
    oldTime = newTime;
    animationTime += speed * deltaTime;

    canvas.fillScreen(ST77XX_BLACK);

    int32_t ang1Start = animationTime;
    int32_t ang2Start = animationTime;
    int32_t scaleMul = scale;
    for (int i = 0; i < CURVECOUNT; i += CURVESTEP)
    {
        uint32_t red = (255 * i) / (CURVECOUNT - CURVESTEP);
        int32_t x = 0;
        int32_t y = 0;
        for (int j = 0; j < ITERATIONS; ++j)
        {
            uint32_t green = (255 * j) / (ITERATIONS - 1);
            uint32_t blue = (510 - (red + green)) >> 1;

            int32_t values1 = sinTable[(ang1Start + x) & (SINTABLEENTRIES - 1)];
            int32_t values2 = sinTable[(ang2Start + y) & (SINTABLEENTRIES - 1)];
            x = (int32_t)(int16_t)values1 + (int32_t)(int16_t)values2;
            y = (values1 >> 16) + (values2 >> 16);
            int32_t pX = (x * scaleMul + xOffset) >> SINTABLEPOWER;
            int32_t pY = (y * scaleMul + yOffset) >> SINTABLEPOWER;
            if (pX >= -SCREENWIDTH / 2 && pX < SCREENWIDTH / 2 && pY >= -SCREENHEIGHT / 2 && pY < SCREENHEIGHT / 2)
            {
                screenCentre[pY * SCREENWIDTH + pX] = ((red >> 3) << (5+6)) | ((green >> 2) << 5) | (blue >> 3);
            }
        }
        ang1Start += ANG1INC;
        ang2Start += ANG2INC;
    }

    tft.drawRGBBitmap(0, 0, buffer, 128, 128);

    controls();

    uint32_t displayEnd = micros();
    Serial.printf("Frame time: %lu ms\n", deltaTime / 1000);
}