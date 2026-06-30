#ifndef SERIAL_CONSOLE_COMMANDS_H
#define SERIAL_CONSOLE_COMMANDS_H

namespace SerialConsoleCommands {

constexpr char LZERO[] = "lzero";
constexpr char RZERO[] = "rzero";
constexpr char LMAX[]  = "lmax";
constexpr char RMAX[]  = "rmax";
constexpr char ZERO[]  = "zero";   // zero <left|right>
constexpr char MAX[]   = "max";    // max  <left|right>
constexpr char MIN[]   = "min";    // min  <left|right>
constexpr char SHOW[]  = "show";   // show data on|off
constexpr char DATA[]  = "data";
constexpr char SETUP[] = "setup";  // setup on|off
constexpr char SET[]   = "set";    // set min|max <left|right>
constexpr char WRITE[] = "write";  // write eeprom
constexpr char EEPROM[] = "eeprom";
constexpr char HOME[]  = "home";   // run startup-style homing pass
constexpr char ON[]    = "on";
constexpr char OFF[]   = "off";
constexpr char LEFT[]  = "left";
constexpr char RIGHT[] = "right";

}  // namespace SerialConsoleCommands

#endif