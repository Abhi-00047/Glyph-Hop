#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// LCD pins
#define PIN_CS   21
#define PIN_DC   12
#define PIN_RST  42
#define PIN_MO   36
#define PIN_CLK  35
#define PIN_BLK  41
#define BRIGHTNESS 255

// Button pins (active LOW)
#define BTN_SW4  39   // -> RED (Bottom Button - Restart)
#define BTN_SW5  38   // -> BLUE (Center Button - Flap / Start)
#define BTN_SW6  40   // -> YELLOW (Top Button - Pause / Resume)

Adafruit_ST7735 tft = Adafruit_ST7735(PIN_CS, PIN_DC, PIN_MO, PIN_CLK, PIN_RST);

// Rich Arcade RGB565 Palette
#define COLOR_BG         0x0821  // Deep space navy
#define COLOR_HUD_BG     0x10A3  // Sleek dark slate HUD bar
#define COLOR_HUD_BORDER 0x04D3  // Crisp teal divider
#define COLOR_ORANGE     0xFD20
#define COLOR_DARK_CYAN  0x03EF
#define COLOR_LIGHT_CYAN 0x87FF
#define COLOR_GOLD       0xFEA0
#define COLOR_PANEL_BG   0x10A4
#define COLOR_GROUND_1   0x2965
#define COLOR_GROUND_2   0x4A69
#define COLOR_STAR       0x52AA

// Layout Boundaries (Clean separation between HUD, Playfield, and Ground)
const int HUD_HEIGHT   = 14;     // y = 0..13 reserved for HUD
const int PLAY_TOP     = 14;     // y = 14..74 playfield
const int GROUND_Y     = 75;     // y = 75..79 scrolling ground

// Game States
enum GameState {
  START,
  PLAYING,
  PAUSED,
  GAMEOVER
};
GameState gameState = START;

// Bird Constants & Physics
const int BIRD_X = 30;
const int BIRD_SIZE = 8;
const float GRAVITY = 0.38;
const float FLAP_STRENGTH = -3.8;

float birdY = 38.0;
float birdVelocity = 0.0;
int lastBirdY = 38;

// Pipe Constants & Variables
const int PIPE_WIDTH = 16;
const int PIPE_GAP_HEIGHT = 28;
const int PIPE_SPEED = 2;
int pipeX = 160;
int pipeGapY = 28;
int lastPipeX = 160;
int lastPipeGapY = 28;

// Game Scores
int score = 0;
int lastScore = -1;
int highScore = 0;
bool isNewHighScore = false;

// Ground animation phase
int groundOffset = 0;

// Button Debounce & State Management
bool prevBtnSW4 = HIGH;
bool prevBtnSW5 = HIGH;
bool prevBtnSW6 = HIGH;

// Timing Control
unsigned long lastFrameTime = 0;
const int FRAME_DELAY_MS = 20; // Locked 50 FPS

// Forward Declarations for Self-Test
bool checkCollision(float bY, int pX, int pGapY);
void runSelfTests();

void setup() {
  Serial.begin(115200);
  delay(1500);

  // Run core physics self-tests on startup
  runSelfTests();

  pinMode(PIN_BLK, OUTPUT);
  analogWrite(PIN_BLK, BRIGHTNESS);

  pinMode(BTN_SW4, INPUT_PULLUP);
  pinMode(BTN_SW5, INPUT_PULLUP);
  pinMode(BTN_SW6, INPUT_PULLUP);

  tft.initR(INITR_MINI160x80);
  tft.invertDisplay(false);
  tft.setRotation(1); // Landscape 160x80

  showStartScreen();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrameTime >= FRAME_DELAY_MS) {
    lastFrameTime = now;
    handleGameLoop();
  }
}

// Subtle static starfield in playfield background
void drawBackgroundStars() {
  const int stars[][2] = {
    {18, 22}, {48, 58}, {76, 26}, {104, 62}, {132, 30}, {150, 52}, {62, 42}, {118, 20}
  };
  for (int i = 0; i < 8; i++) {
    tft.drawPixel(stars[i][0], stars[i][1], COLOR_STAR);
  }
}

// Animated scrolling ground bar at y = 75..79
void updateGround() {
  tft.drawFastHLine(0, GROUND_Y, 160, COLOR_HUD_BORDER);
  for (int x = 0; x < 160; x += 8) {
    uint16_t c1 = ((x / 8 + groundOffset) % 2 == 0) ? COLOR_GROUND_2 : COLOR_GROUND_1;
    uint16_t c2 = ((x / 8 + groundOffset) % 2 == 0) ? COLOR_GROUND_1 : COLOR_GROUND_2;
    tft.fillRect(x, GROUND_Y + 1, 8, 2, c1);
    tft.fillRect(x, GROUND_Y + 3, 8, 2, c2);
  }
  groundOffset = (groundOffset + 1) % 2;
}

// Top HUD Bar (y = 0..12) - Completely isolated from pipes & bird
void drawTopHUD(int currentScore, int bestScore) {
  tft.fillRect(0, 0, 160, HUD_HEIGHT - 1, COLOR_HUD_BG);
  tft.drawFastHLine(0, HUD_HEIGHT - 1, 160, COLOR_HUD_BORDER);

  // Left: Mini Title
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LIGHT_CYAN);
  tft.setCursor(4, 3);
  tft.print("GLYPH HOP");

  // Center-Right: Best Score
  tft.setTextColor(COLOR_GOLD);
  tft.setCursor(76, 3);
  tft.print("HI:");
  tft.print(bestScore);

  // Far-Right: Clean Score Badge
  tft.fillRoundRect(122, 1, 36, 11, 2, COLOR_BG);
  tft.drawRoundRect(122, 1, 36, 11, 2, COLOR_DARK_CYAN);
  tft.setTextColor(ST77XX_WHITE);
  int scoreX = (currentScore >= 100) ? 131 : ((currentScore >= 10) ? 134 : 137);
  tft.setCursor(scoreX, 3);
  tft.print(currentScore);
}

// Updates ONLY the score digits inside the top-right badge (zero full-bar redraw)
void updateHUDScoreOnly(int currentScore) {
  tft.fillRect(124, 2, 32, 9, COLOR_BG);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  int scoreX = (currentScore >= 100) ? 131 : ((currentScore >= 10) ? 134 : 137);
  tft.setCursor(scoreX, 3);
  tft.print(currentScore);
}

// Draw detailed player sprite (8x8 body + 2px beak)
void drawPlayer(int x, int y, bool flapUp) {
  tft.fillRect(x + 1, y, BIRD_SIZE - 2, BIRD_SIZE, COLOR_ORANGE);
  tft.fillRect(x, y + 1, BIRD_SIZE, BIRD_SIZE - 2, COLOR_ORANGE);
  tft.drawFastHLine(x + 2, y + 1, 4, ST77XX_YELLOW);

  // Eye
  tft.fillRect(x + 4, y + 1, 3, 3, ST77XX_WHITE);
  tft.drawPixel(x + 6, y + 2, ST77XX_BLACK);

  // Beak
  tft.fillRect(x + BIRD_SIZE, y + 4, 2, 2, ST77XX_RED);

  // Animated Wing
  if (flapUp) {
    tft.fillRect(x + 1, y + 2, 3, 2, ST77XX_WHITE);
  } else {
    tft.fillRect(x + 1, y + 5, 3, 2, ST77XX_YELLOW);
  }
}

void clearPlayer(int x, int y) {
  tft.fillRect(x, y, BIRD_SIZE + 2, BIRD_SIZE, COLOR_BG);
}

// Helper: Draw a single 1px vertical slice of a pipe at screen coordinate `sx`
// `relX` is the offset from pipe's `pipeX` (-1 to PIPE_WIDTH)
void drawPipeSlice(int sx, int relX, int gapY) {
  if (sx < 0 || sx >= 160) return;

  int topCapY = gapY - 5;
  int botCapY = gapY + PIPE_GAP_HEIGHT;
  int botShaftY = botCapY + 5;

  // Lip overhangs (-1 and PIPE_WIDTH) only draw the gold caps
  if (relX == -1 || relX == PIPE_WIDTH) {
    if (topCapY >= PLAY_TOP) {
      tft.fillRect(sx, topCapY, 1, 5, COLOR_GOLD);
      tft.drawPixel(sx, topCapY, ST77XX_YELLOW);
      tft.drawPixel(sx, topCapY + 4, COLOR_ORANGE);
    }
    if (botCapY < GROUND_Y) {
      int capH = min(5, GROUND_Y - botCapY);
      tft.fillRect(sx, botCapY, 1, capH, COLOR_GOLD);
      tft.drawPixel(sx, botCapY, ST77XX_YELLOW);
      if (capH == 5) tft.drawPixel(sx, botCapY + 4, COLOR_ORANGE);
    }
    return;
  }

  // Choose shaft shading color based on column across pipe width
  uint16_t shaftColor = ST77XX_CYAN;
  if (relX == 2 || relX == 3) shaftColor = COLOR_LIGHT_CYAN;       // Left highlight
  else if (relX >= PIPE_WIDTH - 3) shaftColor = COLOR_DARK_CYAN;   // Right shadow

  // Upper shaft (starts strictly below HUD_HEIGHT)
  if (topCapY > PLAY_TOP) {
    tft.drawFastVLine(sx, PLAY_TOP, topCapY - PLAY_TOP, shaftColor);
  }
  // Upper cap
  if (topCapY >= PLAY_TOP) {
    tft.fillRect(sx, topCapY, 1, 5, COLOR_GOLD);
    tft.drawPixel(sx, topCapY, ST77XX_YELLOW);
    tft.drawPixel(sx, topCapY + 4, COLOR_ORANGE);
  }

  // Lower cap
  if (botCapY < GROUND_Y) {
    int capH = min(5, GROUND_Y - botCapY);
    tft.fillRect(sx, botCapY, 1, capH, COLOR_GOLD);
    tft.drawPixel(sx, botCapY, ST77XX_YELLOW);
    if (capH == 5) tft.drawPixel(sx, botCapY + 4, COLOR_ORANGE);
  }
  // Lower shaft (stops strictly above GROUND_Y)
  if (botShaftY < GROUND_Y) {
    tft.drawFastVLine(sx, botShaftY, GROUND_Y - botShaftY, shaftColor);
  }
}

// Helper: Erase a single 1px vertical slice in the playfield
void clearPlayfieldSlice(int sx) {
  if (sx < 0 || sx >= 160) return;
  tft.drawFastVLine(sx, PLAY_TOP, GROUND_Y - PLAY_TOP, COLOR_BG);
}

// Full pipe redraw (used only when spawning a new pipe or resuming from pause)
void drawFullPipeComplete(int x, int gapY) {
  for (int relX = -1; relX <= PIPE_WIDTH; relX++) {
    drawPipeSlice(x + relX, relX, gapY);
  }
}

// Ultra-fast Delta Pipe Update: Only erases trailing pixels and draws leading pixels
void updatePipeSmooth(int oldX, int newX, int gapY) {
  // Erase trailing edge on the right (oldX + PIPE_WIDTH down to newX + PIPE_WIDTH + 1)
  for (int sx = newX + PIPE_WIDTH + 1; sx <= oldX + PIPE_WIDTH; sx++) {
    clearPlayfieldSlice(sx);
  }
  // Redraw the new right lip overhang
  drawPipeSlice(newX + PIPE_WIDTH, PIPE_WIDTH, gapY);

  // Draw new leading edge on the left (newX - 1 up to oldX - 1)
  for (int sx = newX - 1; sx <= oldX - 1; sx++) {
    drawPipeSlice(sx, sx - newX, gapY);
  }
}

// Central state and physics handler
void handleGameLoop() {
  bool btnSW4 = digitalRead(BTN_SW4); // Bottom
  bool btnSW5 = digitalRead(BTN_SW5); // Center
  bool btnSW6 = digitalRead(BTN_SW6); // Top

  bool pressedSW4 = (btnSW4 == LOW && prevBtnSW4 == HIGH);
  bool pressedSW5 = (btnSW5 == LOW && prevBtnSW5 == HIGH);
  bool pressedSW6 = (btnSW6 == LOW && prevBtnSW6 == HIGH);

  prevBtnSW4 = btnSW4;
  prevBtnSW5 = btnSW5;
  prevBtnSW6 = btnSW6;

  switch (gameState) {
    case START: {
      if (pressedSW5) {
        initGame();
      }
      break;
    }

    case PLAYING: {
      if (pressedSW5) {
        birdVelocity = FLAP_STRENGTH;
      }
      if (pressedSW6) {
        gameState = PAUSED;
        showPauseScreen();
        return;
      }

      // Physics update
      birdVelocity += GRAVITY;
      birdY += birdVelocity;

      pipeX -= PIPE_SPEED;
      bool spawnedNewPipe = false;
      if (pipeX < -PIPE_WIDTH - 1) {
        // Clean up any leftover pixels at x=0
        clearPlayfieldSlice(0);
        pipeX = 160;
        pipeGapY = random(PLAY_TOP + 6, GROUND_Y - PIPE_GAP_HEIGHT - 6);
        spawnedNewPipe = true;
      }

      // Score check
      if (lastPipeX + PIPE_WIDTH >= BIRD_X && pipeX + PIPE_WIDTH < BIRD_X) {
        score++;
      }

      // Collision detection
      if (checkCollision(birdY, pipeX, pipeGapY)) {
        if (score > highScore) {
          highScore = score;
          isNewHighScore = true;
        } else {
          isNewHighScore = false;
        }
        gameState = GAMEOVER;
        showGameOverScreen();
        return;
      }

      // 1. Smooth Delta Pipe Render
      if (spawnedNewPipe) {
        drawFullPipeComplete(pipeX, pipeGapY);
      } else {
        updatePipeSmooth(lastPipeX, pipeX, pipeGapY);
      }
      lastPipeX = pipeX;
      lastPipeGapY = pipeGapY;

      // 2. Restore stars if pipe just passed them, then draw player
      drawBackgroundStars();
      if ((int)birdY != lastBirdY) {
        clearPlayer(BIRD_X, lastBirdY);
        drawPlayer(BIRD_X, (int)birdY, birdVelocity < 0);
        lastBirdY = (int)birdY;
      } else {
        drawPlayer(BIRD_X, (int)birdY, birdVelocity < 0);
      }

      // 3. Scroll ground strip
      updateGround();

      // 4. Update HUD score ONLY when score changes
      if (score != lastScore) {
        updateHUDScoreOnly(score);
        lastScore = score;
      }
      break;
    }

    case PAUSED: {
      if (pressedSW6) {
        tft.fillScreen(COLOR_BG);
        drawTopHUD(score, highScore);
        drawBackgroundStars();
        drawFullPipeComplete(pipeX, pipeGapY);
        drawPlayer(BIRD_X, (int)birdY, birdVelocity < 0);
        updateGround();

        lastBirdY = (int)birdY;
        lastPipeX = pipeX;
        lastPipeGapY = pipeGapY;
        lastScore = score;

        gameState = PLAYING;
      }
      break;
    }

    case GAMEOVER: {
      if (pressedSW4) {
        initGame();
      }
      break;
    }
  }
}

// Reset variables to start playing
void initGame() {
  tft.fillScreen(COLOR_BG);
  drawTopHUD(0, highScore);
  drawBackgroundStars();
  updateGround();

  birdY = 38.0;
  birdVelocity = 0.0;
  lastBirdY = (int)birdY;

  pipeX = 160;
  pipeGapY = random(PLAY_TOP + 6, GROUND_Y - PIPE_GAP_HEIGHT - 6);
  lastPipeX = pipeX;
  lastPipeGapY = pipeGapY;

  score = 0;
  lastScore = 0;
  isNewHighScore = false;

  drawPlayer(BIRD_X, (int)birdY, false);
  gameState = PLAYING;
}

// Start Screen
void showStartScreen() {
  tft.fillScreen(COLOR_BG);
  drawBackgroundStars();
  updateGround();

  tft.drawRoundRect(4, 3, 152, 69, 5, COLOR_HUD_BORDER);

  // Decorative mini preview pipe on right side
  tft.fillRect(130, 4, 12, 20, ST77XX_CYAN);
  tft.fillRect(129, 22, 14, 4, COLOR_GOLD);
  tft.fillRect(130, 49, 12, 22, ST77XX_CYAN);
  tft.fillRect(129, 47, 14, 4, COLOR_GOLD);

  // Preview player sprite
  drawPlayer(20, 21, true);

  // Crisp Dual-Tone Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_DARK_CYAN);
  tft.setCursor(37, 13);
  tft.print("GLYPH");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(36, 12);
  tft.print("GLYPH");

  tft.setTextColor(COLOR_ORANGE);
  tft.setCursor(55, 31);
  tft.print("HOP");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(54, 30);
  tft.print("HOP");

  // Action prompt pill
  tft.fillRoundRect(16, 52, 106, 15, 3, COLOR_PANEL_BG);
  tft.drawRoundRect(16, 52, 106, 15, 3, COLOR_DARK_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LIGHT_CYAN);
  tft.setCursor(22, 56);
  tft.print("[CENTER] TO PLAY");
}

// Pause Modal
void showPauseScreen() {
  tft.fillRoundRect(22, 20, 116, 46, 5, COLOR_PANEL_BG);
  tft.drawRoundRect(22, 20, 116, 46, 5, ST77XX_YELLOW);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(45, 26);
  tft.print("PAUSED");

  tft.setTextColor(COLOR_LIGHT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(35, 49);
  tft.print("[TOP] TO RESUME");
}

// Game Over Card
void showGameOverScreen() {
  tft.fillScreen(COLOR_BG);
  drawBackgroundStars();
  updateGround();

  // Header Banner
  tft.setTextSize(2);
  tft.setTextColor(COLOR_ORANGE);
  tft.setCursor(27, 6);
  tft.print("GAME OVER");
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(26, 5);
  tft.print("GAME OVER");

  // Score Card Box
  tft.fillRoundRect(16, 25, 128, 32, 4, COLOR_PANEL_BG);
  tft.drawRoundRect(16, 25, 128, 32, 4, COLOR_HUD_BORDER);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_LIGHT_CYAN);
  tft.setCursor(24, 31);
  tft.print("SCORE");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(68, 31);
  tft.print(score);

  tft.setTextColor(COLOR_LIGHT_CYAN);
  tft.setCursor(24, 44);
  tft.print("BEST");
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(68, 44);
  tft.print(highScore);

  if (isNewHighScore && score > 0) {
    tft.fillRoundRect(102, 42, 36, 11, 2, COLOR_GOLD);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(105, 44);
    tft.print("NEW!");
  }

  // Bottom Restart Prompt
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(23, 63);
  tft.print("[BOTTOM] PLAY AGAIN");
}

// Bounding box collision checker (Respects HUD & Ground boundaries)
bool checkCollision(float bY, int pX, int pGapY) {
  if (bY < PLAY_TOP || bY + BIRD_SIZE > GROUND_Y) {
    return true;
  }

  bool horizontalOverlap = (BIRD_X + BIRD_SIZE > pX) && (BIRD_X < pX + PIPE_WIDTH);
  if (horizontalOverlap) {
    bool verticalOverlap = (bY < pGapY) || (bY + BIRD_SIZE > pGapY + PIPE_GAP_HEIGHT);
    if (verticalOverlap) {
      return true;
    }
  }

  return false;
}

// Target-runnable firmware self-tests
void runSelfTests() {
  Serial.println("--- Running Firmware Self-Tests ---");
  int passed = 0, failed = 0;

  if (!checkCollision(34, 30, 28)) passed++; else failed++;
  if (checkCollision(22, 30, 28)) passed++; else failed++;
  if (checkCollision(52, 30, 28)) passed++; else failed++;
  if (checkCollision(PLAY_TOP - 1, 100, 28)) passed++; else failed++;
  if (checkCollision(GROUND_Y - BIRD_SIZE + 1, 100, 28)) passed++; else failed++;

  Serial.print("Passed: "); Serial.println(passed);
  Serial.print("Failed: "); Serial.println(failed);
  Serial.println("-----------------------------------");
}
