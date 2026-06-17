#include <screens.h>

Adafruit_ILI9341 tft = Adafruit_ILI9341(CS, DC);
int yOffset = 0;

mainScreen::mainScreen(){
    diff = false;
    AWD = false;
    mode = OFF;
    angle = 1;
    currentGear = PARK;
    topPageCount = 0;      // No pages configured yet
    currentTopPage = 0;    // Start on page 0 after setup
}

void mainScreen::init(){
    tft.begin();                          // Initialize ILI9341 display
    tft.setRotation(2);                   // Set physical screen orientation
    tft.fillScreen(ILI9341_BLACK);        // Clear the display to black
}

void mainScreen::updateAll(){
    // Redraw the full dashboard page state.
    tft.fillScreen(ILI9341_BLACK);       // Clear entire display before redraw.
    renderTopFieldPage();                // Draw the header text fields.
    skeleton();                          // Draw the wireframe vehicle body.

    // Draw indicator states and vehicle status blocks.
    if(diff){ diffLock(); } else { diffUnlock(); }
    if(AWD){ AWD_engage(); } else { AWD_disengage(); }

    // Draw the currently selected rear-steer visualization.
    if(mode == OFF){ rearSteerOff(); }
    if(mode == CRAB){ rearSteerCrab(); }
    if(mode == NORMAL){ rearSteerNormal(); }

    shift(currentGear);                  // Draw the current gear indicator.
}

void mainScreen::renderTopFieldPage(){
    const uint16_t background = ILI9341_BLACK;

    // Clear the top header area and prepare text layout.
    tft.fillRect(0, 0, 240, 80, background);
    tft.setTextWrap(false);

    if(topPageCount == 0 || currentTopPage >= topPageCount){
        tft.setTextColor(ILI9341_RED);
        tft.setTextSize(2);
        tft.setCursor(8, 8);
        tft.print("NO TOP FIELD PAGES");
        return;
    }

    TopFieldPage& page = topPages[currentTopPage];

    // Draw the page title in a larger, easy-to-read font.
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(4, 8);
    tft.print(page.title);

    // Draw each row of label/value pairs below the title.
    for(uint8_t i = 0; i < page.fieldCount && i < MAX_TOP_FIELDS; ++i){
        drawTopFieldRow(i, page.labels[i], page.values[i]);
    }
}

void mainScreen::drawTopFieldRow(uint8_t row, const char* label, const char* value){
    // Vertical layout values for the header rows.
    const uint8_t rowHeight = 18;
    const uint8_t y = 34 + row * rowHeight;
    const uint8_t labelX = 4;
    const uint8_t valueX = 120;

    // Draw the field label on the left.
    tft.setCursor(labelX, y);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(label);

    // Draw the dynamic field value on the right.
    tft.setCursor(valueX, y);
    tft.setTextColor(ILI9341_CYAN);
    tft.print(value);
}

bool mainScreen::validateTopFieldIndexes(uint16_t pageIndex, uint8_t fieldIndex) const{
    // Ensure the requested page and field exist before writing values.
    if(pageIndex >= topPageCount){
        return false;
    }
    return fieldIndex < topPages[pageIndex].fieldCount;
}

void mainScreen::copyTopFieldText(char* dest, const char* source){
    // Safe bounded copy for fixed-width character buffers.
    const size_t maxLen = MAX_FIELD_TEXT - 1;
    size_t i = 0;
    while(i < maxLen && source[i] != '\0'){
        dest[i] = source[i];
        ++i;
    }
    dest[i] = '\0';
}

bool mainScreen::addTopFieldPage(const char title[MAX_FIELD_TEXT], const char labels[][MAX_FIELD_TEXT], uint8_t fieldCount){
    // Add a new header page with the provided title and label names only.
    // Values will be blank until updated by CAN or runtime logic.
    if(fieldCount == 0 || fieldCount > MAX_TOP_FIELDS || topPageCount >= MAX_TOP_FIELD_PAGES){
        return false;
    }

    TopFieldPage& newPage = topPages[topPageCount];
    newPage.fieldCount = fieldCount;
    copyTopFieldText(newPage.title, title);
    for(uint8_t i = 0; i < fieldCount; ++i){
        copyTopFieldText(newPage.labels[i], labels[i]);
        newPage.values[i][0] = '\0';        // Initialize value as empty
    }

    topPageCount++;
    return true;
}

bool mainScreen::removeTopFieldPage(uint8_t pageIndex){
    // Remove the selected header page and slide later pages up to fill the gap.
    if(pageIndex >= topPageCount){
        return false;
    }

    for(uint8_t i = pageIndex; i + 1 < topPageCount; ++i){
        topPages[i] = topPages[i + 1];
    }
    topPageCount--;
    if(currentTopPage >= topPageCount && topPageCount > 0){
        currentTopPage = topPageCount - 1;
    }
    return true;
}

bool mainScreen::setTopFieldPage(uint8_t pageIndex){
    if(pageIndex >= topPageCount){
        return false;
    }
    currentTopPage = pageIndex;
    return true;
}

bool mainScreen::nextTopFieldPage(){
    // Advance to the next header page, wrapping back to the first page.
    if(topPageCount == 0){
        return false;
    }
    currentTopPage = (currentTopPage + 1) % topPageCount;
    return true;
}

bool mainScreen::prevTopFieldPage(){
    // Move to the previous header page, wrapping to the last page.
    if(topPageCount == 0){
        return false;
    }
    currentTopPage = (currentTopPage + topPageCount - 1) % topPageCount;
    return true;
}

bool mainScreen::updateTopField(uint8_t pageIndex, uint8_t fieldIndex, const char* label, const char* value){
    if(!validateTopFieldIndexes(pageIndex, fieldIndex)){
        return false;
    }
    copyTopFieldText(topPages[pageIndex].labels[fieldIndex], label);
    copyTopFieldText(topPages[pageIndex].values[fieldIndex], value);
    return true;
}

void mainScreen::splashScreen(int time){
    tft.drawRGBBitmap(0,50,(uint16_t*)gear,240,240); //https://notisrac.github.io/FileToCArray/
    delay(time);
    tft.fillScreen(ILI9341_BLACK);//clear screen
}

void mainScreen::splashScreen2(int time){
    tft.fillScreen(ILI9341_WHITE);
    tft.drawRGBBitmap(0,28,(uint16_t*)frame,240,263); //https://notisrac.github.io/FileToCArray/
    delay(time);
    tft.fillScreen(ILI9341_BLACK);//clear screen
}

void mainScreen::skeleton(){
    tft.drawFastVLine(35,160+yOffset,70,ILI9341_WHITE); //chasis skeleton
    tft.drawFastVLine(205,160+yOffset,25,ILI9341_WHITE);
    tft.drawFastVLine(205,205+yOffset,25,ILI9341_WHITE);
    tft.drawFastHLine(35,195+yOffset,70,ILI9341_WHITE);
    tft.drawFastHLine(135,195+yOffset,60,ILI9341_WHITE);
}

void mainScreen::diffLock(){
    diff = true;
    tft.fillCircle(205,195+yOffset,10,ILI9341_WHITE); // diff lock    
}

void mainScreen::diffUnlock(){
    diff = false;
    tft.fillCircle(205,195+yOffset,10,ILI9341_BLACK); //diff unlock
    tft.drawCircle(205,195+yOffset,10,ILI9341_WHITE);
}

void mainScreen::AWD_engage(){
    AWD = true;
    tft.fillRoundRect(105,180+yOffset,30,30,4,ILI9341_WHITE); //AWD engage
}

void mainScreen::AWD_disengage(){
    AWD = false;
    tft.fillRoundRect(105,180+yOffset,30,30,4,ILI9341_BLACK); //AWD disengage
    tft.drawRoundRect(105,180+yOffset,30,30,4,ILI9341_WHITE);
}

void mainScreen::rearSteerOff(){
    if(mode==OFF){
        rearSteerOffInvert();
    }
    if(mode==NORMAL){
        rearSteerNormalInvert();
    }
    if(mode==CRAB){
        rearSteerCrabInvert();
    }
    tft.drawRect(15,140+yOffset,40,20,ILI9341_WHITE); // straight front wheels
    tft.drawRect(15,230+yOffset,40,20,ILI9341_WHITE);

    tft.drawRect(185,140+yOffset,40,20,ILI9341_WHITE); // straight rear wheels
    tft.drawRect(185,230+yOffset,40,20,ILI9341_WHITE);
    mode = OFF;
}

void mainScreen::rearSteerCrab(){
    if(mode==OFF){
        rearSteerOffInvert();
    }
    if(mode==NORMAL){
        rearSteerNormalInvert();
    }
    if(mode==CRAB){
        rearSteerCrabInvert();
    }
    tft.drawLine(8,150+yOffset,45,133+yOffset,ILI9341_WHITE); //front right turned left
    tft.drawLine(45,133+yOffset,53,152+yOffset,ILI9341_WHITE);
    tft.drawLine(53,152+yOffset,17,168+yOffset,ILI9341_WHITE);
    tft.drawLine(17,168+yOffset,8,150+yOffset,ILI9341_WHITE);

    tft.drawLine(17,238+yOffset,53,222+yOffset,ILI9341_WHITE); //front left turned left
    tft.drawLine(53,222+yOffset,62,240+yOffset,ILI9341_WHITE);
    tft.drawLine(62,240+yOffset,25,257+yOffset,ILI9341_WHITE);
    tft.drawLine(25,257+yOffset,17,238+yOffset,ILI9341_WHITE);

    tft.drawLine(178,150+yOffset,215,133+yOffset,ILI9341_WHITE); //rear right turned left
    tft.drawLine(215,133+yOffset,223,152+yOffset,ILI9341_WHITE);
    tft.drawLine(223,152+yOffset,187,168+yOffset,ILI9341_WHITE);
    tft.drawLine(187,168+yOffset,178,150+yOffset,ILI9341_WHITE);

    tft.drawLine(187,238+yOffset,223,222+yOffset,ILI9341_WHITE); //rear left turned left
    tft.drawLine(223,222+yOffset,232,240+yOffset,ILI9341_WHITE);
    tft.drawLine(232,240+yOffset,195,257+yOffset,ILI9341_WHITE);
    tft.drawLine(195,257+yOffset,187,238+yOffset,ILI9341_WHITE);
    mode = CRAB;
}

void mainScreen::rearSteerNormal(){
    if(mode==OFF){
        rearSteerOffInvert();
    }
    if(mode==NORMAL){
        rearSteerNormalInvert();
    }
    if(mode==CRAB){
        rearSteerCrabInvert();
    }
    tft.drawLine(8,150+yOffset,45,133+yOffset,ILI9341_WHITE); //front right turned left
    tft.drawLine(45,133+yOffset,53,152+yOffset,ILI9341_WHITE);
    tft.drawLine(53,152+yOffset,17,168+yOffset,ILI9341_WHITE);
    tft.drawLine(17,168+yOffset,8,150+yOffset,ILI9341_WHITE);

    tft.drawLine(17,238+yOffset,53,222+yOffset,ILI9341_WHITE); //front left turned left
    tft.drawLine(53,222+yOffset,62,240+yOffset,ILI9341_WHITE);
    tft.drawLine(62,240+yOffset,25,257+yOffset,ILI9341_WHITE);
    tft.drawLine(25,257+yOffset,17,238+yOffset,ILI9341_WHITE);

    tft.drawLine(195,133+yOffset,232,150+yOffset,ILI9341_WHITE); //rear right turned right
    tft.drawLine(232,150+yOffset,223,168+yOffset,ILI9341_WHITE);
    tft.drawLine(223,168+yOffset,187,152+yOffset,ILI9341_WHITE);
    tft.drawLine(187,152+yOffset,195,133+yOffset,ILI9341_WHITE);

    tft.drawLine(187,222+yOffset,223,238+yOffset,ILI9341_WHITE); //rear left turned right
    tft.drawLine(223,238+yOffset,215,257+yOffset,ILI9341_WHITE);
    tft.drawLine(215,257+yOffset,178,240+yOffset,ILI9341_WHITE);
    tft.drawLine(178,240+yOffset,187,222+yOffset,ILI9341_WHITE);
    mode = NORMAL;
}

void mainScreen::rearSteerOffInvert(){
    tft.drawRect(15,140+yOffset,40,20,ILI9341_BLACK); // straight front wheels
    tft.drawRect(15,230+yOffset,40,20,ILI9341_BLACK);

    tft.drawRect(185,140+yOffset,40,20,ILI9341_BLACK); // straight rear wheels
    tft.drawRect(185,230+yOffset,40,20,ILI9341_BLACK);
}

void mainScreen::rearSteerCrabInvert(){
    tft.drawLine(8,150+yOffset,45,133+yOffset,ILI9341_BLACK); //front right turned left
    tft.drawLine(45,133+yOffset,53,152+yOffset,ILI9341_BLACK);
    tft.drawLine(53,152+yOffset,17,168+yOffset,ILI9341_BLACK);
    tft.drawLine(17,168+yOffset,8,150+yOffset,ILI9341_BLACK);

    tft.drawLine(17,238+yOffset,53,222+yOffset,ILI9341_BLACK); //front left turned left
    tft.drawLine(53,222+yOffset,62,240+yOffset,ILI9341_BLACK);
    tft.drawLine(62,240+yOffset,25,257+yOffset,ILI9341_BLACK);
    tft.drawLine(25,257+yOffset,17,238+yOffset,ILI9341_BLACK);

    tft.drawLine(178,150+yOffset,215,133+yOffset,ILI9341_BLACK); //rear right turned left
    tft.drawLine(215,133+yOffset,223,152+yOffset,ILI9341_BLACK);
    tft.drawLine(223,152+yOffset,187,168+yOffset,ILI9341_BLACK);
    tft.drawLine(187,168+yOffset,178,150+yOffset,ILI9341_BLACK);

    tft.drawLine(187,238+yOffset,223,222+yOffset,ILI9341_BLACK); //rear left turned left
    tft.drawLine(223,222+yOffset,232,240+yOffset,ILI9341_BLACK);
    tft.drawLine(232,240+yOffset,195,257+yOffset,ILI9341_BLACK);
    tft.drawLine(195,257+yOffset,187,238+yOffset,ILI9341_BLACK);
}

void mainScreen::rearSteerNormalInvert(){
    tft.drawLine(8,150+yOffset,45,133+yOffset,ILI9341_BLACK); //front right turned left
    tft.drawLine(45,133+yOffset,53,152+yOffset,ILI9341_BLACK);
    tft.drawLine(53,152+yOffset,17,168+yOffset,ILI9341_BLACK);
    tft.drawLine(17,168+yOffset,8,150+yOffset,ILI9341_BLACK);

    tft.drawLine(17,238+yOffset,53,222+yOffset,ILI9341_BLACK); //front left turned left
    tft.drawLine(53,222+yOffset,62,240+yOffset,ILI9341_BLACK);
    tft.drawLine(62,240+yOffset,25,257+yOffset,ILI9341_BLACK);
    tft.drawLine(25,257+yOffset,17,238+yOffset,ILI9341_BLACK);

    tft.drawLine(195,133+yOffset,232,150+yOffset,ILI9341_BLACK); //rear right turned right
    tft.drawLine(232,150+yOffset,223,168+yOffset,ILI9341_BLACK);
    tft.drawLine(223,168+yOffset,187,152+yOffset,ILI9341_BLACK);
    tft.drawLine(187,152+yOffset,195,133+yOffset,ILI9341_BLACK);

    tft.drawLine(187,222+yOffset,223,238+yOffset,ILI9341_BLACK); //rear left turned right
    tft.drawLine(223,238+yOffset,215,257+yOffset,ILI9341_BLACK);
    tft.drawLine(215,257+yOffset,178,240+yOffset,ILI9341_BLACK);
    tft.drawLine(178,240+yOffset,187,222+yOffset,ILI9341_BLACK);
}

void mainScreen::shift(GEAR inputGear){
  tft.drawFastHLine(0,270,240,ILI9341_WHITE);
  tft.drawChar(12,280,'P',ILI9341_WHITE,0,5);
  tft.drawChar(57,280,'R',ILI9341_WHITE,0,5);
  tft.drawChar(102,280,'N',ILI9341_WHITE,0,5);
  tft.drawChar(157,280,'L',ILI9341_WHITE,0,5);
  tft.drawChar(202,280,'D',ILI9341_WHITE,0,5);

  if(inputGear==PARK){
    tft.drawChar(12,280,'P',ILI9341_RED,0,5);
  }
  if(inputGear==REVERSE){
    tft.drawChar(57,280,'R',ILI9341_RED,0,5);
  }
  if(inputGear==NEUTRAL){
    tft.drawChar(102,280,'N',ILI9341_RED,0,5);
  }
  if(inputGear==LOWGEAR){
    tft.drawChar(157,280,'L',ILI9341_RED,0,5);
  }
  if(inputGear==DRIVE){
    tft.drawChar(202,280,'D',ILI9341_RED,0,5);
  }
}

void mainScreen::unlockDiffScreen(int time){
    tft.fillScreen(ILI9341_BLACK);//clear screen
    tft.setCursor(15,35);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(3);
    tft.print("DIFFERENTIAL");
    tft.drawBitmap(61,100,(uint8_t*)unlock,118,163,ILI9341_WHITE); //https://javl.github.io/image2cpp/
    delay(time);
    updateAll();
}

void mainScreen::lockDiffScreen(int time){
    tft.fillScreen(ILI9341_BLACK);//clear screen
    tft.setCursor(15,35);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(3);
    tft.print("DIFFERENTIAL");
    tft.drawBitmap(61,100,(uint8_t*)lock,118,163,ILI9341_WHITE); //https://javl.github.io/image2cpp/
    delay(time);
    updateAll();
}

void mainScreen::AWD_engageScreen(int time){
    tft.fillScreen(ILI9341_BLACK);//clear screen
    tft.setCursor(90,250);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(5);
    tft.print("ON");
    tft.drawBitmap(5,15,(uint8_t*)FOURWD,230,200,ILI9341_WHITE); //https://javl.github.io/image2cpp/
    delay(time);
    updateAll();
}

void mainScreen::AWD_disengageScreen(int time){
    tft.fillScreen(ILI9341_BLACK);//clear screen
    tft.setCursor(80,250);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(5);
    tft.print("OFF");
    tft.drawBitmap(5,15,(uint8_t*)FOURWD,230,200,ILI9341_WHITE); //https://javl.github.io/image2cpp/
    delay(time);
    updateAll();
}

void mainScreen::rear_calibration_screen(int time){
    tft.fillScreen(ILI9341_WHITE);
    tft.drawRGBBitmap(37,5,(uint16_t*)rear_calibration,165,310); //https://notisrac.github.io/FileToCArray/
    tft.setCursor(20,290);
    tft.setTextColor(ILI9341_BLACK);
    tft.setTextSize(3);
    tft.print("CALIBRATING");
    delay(time);
    updateAll();
}


