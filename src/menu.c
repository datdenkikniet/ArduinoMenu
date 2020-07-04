#include <avr/io.h>
#include "menu.h"
#include "io/iocontrol.h"

I2CInfo i2cInfo = {0x00, 0x00};

MenuState showMainMenu(int *printed);

MenuState showSecondaryMenu(int *printed);

MenuState showLedMenu(int *printed);

MenuState showI2CMenu(int *printed, I2CInfo *info);

MenuState showI2CSetMenu(int *printed, uint8_t *valueToSet, MenuState originalState, SetState *setState);

void printMenu(const char *const *strings, int amtOfStrings, int *printed, CharResult *inputResult);

void printTemperature();

void showMenu(MenuState *state, int *continueRunning) {
    static SetState setState = {0, 0};
    static uint32_t counter = 0;
    counter++;
    MenuState currentState = *state;
    static int printed = 0;

    switch (currentState) {
        case UNINITIALIZED:
        case MAIN_MENU:
            *state = showMainMenu(&printed);
            break;
        case SECONDARY_MENU:
            *state = showSecondaryMenu(&printed);
            break;
        case TOGGLE_LED_MENU:
            *state = showLedMenu(&printed);
            break;
        case I2C_MENU:
            *state = showI2CMenu(&printed, &i2cInfo);
            break;
        case I2C_MENU_SET_SLAVE:
            *state = showI2CSetMenu(&printed, &i2cInfo.slaveAddress, *state, &setState);
            break;
        case I2C_MENU_SET_ADDR:
            *state = showI2CSetMenu(&printed, &i2cInfo.dataAddress, *state, &setState);
            break;
        case EXIT:
        default:
            printLine("Exiting menu...");
            *continueRunning = 0;
            break;
    }
}

void sendUnknownCharMsg(char character) {
    static char *data = "Unknown option \" \"";
    data[16] = character;
    printLine(data);
}

void printMenu(const char *const *strings, const int amtOfStrings, int *printed, CharResult *inputResult) {
    if (!*printed) {
        for (int i = 0; i < amtOfStrings; i++) {
            printLine(strings[i]);
        }
        *printed = 1;
    }

    getChar(inputResult);
    if (inputResult->success) {
        *printed = 0;
    }
}


MenuState showMainMenu(int *printed) {
    const char *const menuitems[] =
            {"Main menu",
             "Options:",
             "a - print some other text",
             "b - go to the 2nd menu",
             "c - reprint the main menu",
             "l - go to the LED menu",
             "p - print the temperature",
             "r - go to i2c menu",
             "q - exit the program"};

    CharResult input;
    printMenu(menuitems, sizeof(menuitems) / sizeof(char **), printed, &input);

    if (!input.success) {
        return MAIN_MENU;
    }

    switch (input.value) {
        case 'a':
            printLine("Here we print something");
            return MAIN_MENU;
        case 'b':
            return SECONDARY_MENU;
        case 'c':
            return MAIN_MENU;
        case 'q':
            return EXIT;
        case 'l':
            return TOGGLE_LED_MENU;
        case 'p':
            printTemperature();
            return MAIN_MENU;
        case 'r':
            return I2C_MENU;
        default: {
            sendUnknownCharMsg(input.value);
            return MAIN_MENU;
        }
    }
}

MenuState showI2CMenu(int *printed, I2CInfo *info) {
    const char *const menuItems[] = {
            "I2C Menu:",
            "Options:",
            "s - set slave address",
            "d - set data address",
            "r - read a byte",
            "q - return to the main menu"
    };
    CharResult input;
    if (!*printed) {
        printStr("SLV addr: ");
        sendHexInt(info->slaveAddress);
        printStr(NEWLINE);
        printStr("DATA addr: ");
        sendHexInt(info->dataAddress);
        printStr(NEWLINE);
    }
    printMenu(menuItems, sizeof(menuItems) / sizeof(char **), printed, &input);
    if (!input.success) {
        return I2C_MENU;
    }
    switch (input.value) {
        case 's':
            return I2C_MENU_SET_SLAVE;
        case 'd':
            return I2C_MENU_SET_ADDR;
        case 'r': {
            uint8_t value;
            twireadsingle(i2cInfo.slaveAddress, i2cInfo.dataAddress, &value);
            printStr("DATA value: 0x");
            sendHexInt(value);
            printStr(NEWLINE);
            return I2C_MENU;
        }
        case 'q':
            return MAIN_MENU;
        default:
            sendUnknownCharMsg(input.value);
            return I2C_MENU;
    }
}

MenuState showI2CSetMenu(int *printed, uint8_t *toSet, MenuState originalState, SetState *setState) {
    if (!*printed){
        printLine("Input 2-digit hexadecimal number (or q to quit)");
        *printed = 1;
    }
    CharResult input;
    getChar(&input);
    if (!input.success) {
        return originalState;
    }

    int8_t value = hexToInt(input.value);

    // If input is q, go back to I2C menu. If it's not q, and invalid, print invalid output and return to
    // original state.
    if (input.value == 'q') {
        setState->readFirstChar = 0;
        return I2C_MENU;
    } else if (value == -1) {
        printLine("Invalid input. (Must be 0-F)");
        setState->readFirstChar = 0;
        *printed = 0;
        return originalState;
    }
    // Determine the mask and shift values based on whether it's the first or second character that is read
    int8_t mask = 0xF0;
    int8_t shift = 0;

    if (!setState->readFirstChar) {
        mask = 0x0F;
        shift = 4;
    }

    // Apply mask (to prevent "spill" from previously set values) and add the value, shifted into the right
    // position
    setState->newValue &= mask;
    setState->newValue += value << shift;

    // Return the correct next state, and update readFirstChar appropriately
    // If finished, update the value that is supposed to be updated accordingly
    if (!setState->readFirstChar) {
        setState->readFirstChar = 1;
        return originalState;
    } else {
        *toSet = setState->newValue;
        *printed = 0;
        setState->newValue = 0;
        setState->readFirstChar = 0;
        return I2C_MENU;
    }
}

MenuState showSecondaryMenu(int *printed) {
    const char *const menuItems[] = {
            "Secondary menu",
            "Options:",
            "a - reprint this menu",
            "q - return to the main menu"
    };
    CharResult input;
    printMenu(menuItems, sizeof(menuItems) / sizeof(char **), printed, &input);
    if (!input.success) {
        return SECONDARY_MENU;
    }

    switch (input.value) {
        case 'a':
            return SECONDARY_MENU;
        case 'q':
            return MAIN_MENU;
        default:
            sendUnknownCharMsg(input.value);
            return SECONDARY_MENU;
    }
}

MenuState showLedMenu(int *printed) {
    const char *const menuItems[] = {
            "Led toggle menu",
            "Options:",
            "a - toggle LED",
            "b - turn LED on",
            "c - turn LED off",
            "q - return to the main menu"
    };

    CharResult input;
    printMenu(menuItems, sizeof(menuItems) / sizeof(char **), printed, &input);

    if (!input.success) {
        return TOGGLE_LED_MENU;
    }

    uint8_t portBvalue = PORTB;
    char inChar = input.value;

    if (inChar == 'a') {
        if (!portBvalue) {
            inChar = 'b';
        } else {
            inChar = 'c';
        }
    }

    switch (inChar) {
        case 'b':
            PORTB = 0xFF;
            printLine("Toggled LED on");
            return TOGGLE_LED_MENU;
        case 'c':
            PORTB = 0x00;
            printLine("Toggled LED off");
            return TOGGLE_LED_MENU;
        case 'q':
            return MAIN_MENU;
        default:
            sendUnknownCharMsg(input.value);
            return TOGGLE_LED_MENU;
    }
}

// Print the temperature of a DS3231 connected to the TWI of this arduino
void printTemperature() {
    int8_t temp_msb;
    twireadsingle(DS3231_ADDR, 0x11, &temp_msb);
    uint8_t temp_lsb;
    twireadsingle(DS3231_ADDR, 0x12, &temp_lsb);
    temp_lsb >>= 6;

    const char *data = "Temperature: ";

    printStr(data);
    if (temp_msb < 0) {
        printChar('-');
        temp_msb = temp_msb * -1;
    }

    printChar((temp_msb / 10) + ASCII_DECIMAL_OFFSET);
    printChar((temp_msb % 10) + ASCII_DECIMAL_OFFSET);
    printChar('.');
    if (temp_lsb == 0b00) {
        printLine("00");
    } else if (temp_lsb == 0b01) {
        printLine("25");
    } else if (temp_lsb == 0b10) {
        printLine("50");
    } else if (temp_lsb == 0b11) {
        printLine("75");
    }
}