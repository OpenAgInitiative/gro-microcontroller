/** 
 *  \file communication.cpp
 *  \brief Handles a character based serial communication protocol.
 *  \details See communication.h for details.
 *  \author Jake Rye
 */
#include "communication.h"

void Communication::begin(void) {
  kBaudRate = 9600;
  kEstablishConnectionTimeout = 2000; // milliseconds
  kReceiveTimeout = 5000; // milliseconds
  kStartOfHeaderChar = 1;
  kStartOfTextChar = 2;
  kEndOfTextChar = 3;
  kEndOfTransmissionChar = 4;
  kEnquireChar = 5;
  kAcknowledgeChar = 6; 
  
  Serial.begin(kBaudRate);
  Serial.write(kEnquireChar);  // send enquiry
  uint32_t start_time = millis();
  while (1) {
    if (Serial.available()) { 
      if (Serial.read() == kAcknowledgeChar) { // await acknowledgement
        Serial.write(kAcknowledgeChar); // acknowledge acknowledgementq
        not_connected_ = 0;
        return; // connected!
      }
    }
    if (millis() - start_time > kEstablishConnectionTimeout) {
      String connection_error_message = "Did not establish connection with rPi";
      Serial.println(connection_error_message);
      not_connected_ = 1;
      break; // timed out :(
    }
  }
}

void Communication::send(String outgoing_message) {
  outgoing_message = "{" + outgoing_message + "},";
  
  if (not_connected_) {
    Serial.println(outgoing_message);
  }
  else { // Connected to rPi
    String packed_message = getPackedMessage(outgoing_message);
    Serial.print(packed_message);
  }
}

bool Communication::available(void) {
  return Serial.available() ? 1 : 0;
}

String Communication::receive(void) { 
  String incoming_message = "";
  char incoming_char;
  bool timed_out = 0;
  uint32_t start_time = millis();

  // Handle Incoming Messages
  while (1) {
    delay(10);
    incoming_char = Serial.read();

    if ((not_connected_ && (incoming_char == '\n'))
    		|| (!not_connected_ && (incoming_char == kEndOfTransmissionChar)))  {
      break;
    }
    
    incoming_message += incoming_char;
    
    if (millis() - start_time > kReceiveTimeout) {
      timed_out = 1;
      break;
    }
  }
  if (not_connected_) {
    if (timed_out)
      incoming_message = "Timed Out";
    return incoming_message;
  }
  else {
    if (timed_out)
      return "";
    String unpacked_message =  getUnpackedMessage(incoming_message);
    return unpacked_message;
  }
}

String Communication::getPackedMessage(String message) {
  String packed_message = "";
  String checksum = getChecksum(message);
  packed_message += kStartOfHeaderChar; 
  packed_message += message.length();
  packed_message += kStartOfTextChar; 
  packed_message += message;
  packed_message += kEndOfTextChar; 
  packed_message += checksum; 
  packed_message += kEndOfTransmissionChar;
  return packed_message;
}

String Communication::getChecksum(String message) {
  byte crc = 0x00;
  int len = message.length();
  int counter = 0;
  while (len--) {
    byte extract = message.charAt(counter);
    counter++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return String(crc);
}

String Communication::getUnpackedMessage(String message) {
  // Parse Header
  int message_size = parseHeader(message);

  // Parse Text
  String unpacked_message = parseText(message); 
  
  // Compute & Compare Checksums
  String incoming_checksum = parseFooter(message);
  String computed_checksum = getChecksum(unpacked_message);
  
  return (!checkStartOfHeader(message)
		  || message_size < 0 
		  || unpacked_message == "" // 
		  || message_size != unpacked_message.length()
		  || incoming_checksum != computed_checksum ) ? "" : unpacked_message;
}

bool Communication::checkStartOfHeader(String message) {
  return (message[0] == kStartOfHeaderChar) ? 1 : 0;
}

int Communication::parseHeader(String message) {
  int end_char = message.indexOf(kStartOfTextChar);
  return (end_char < 0) ? -1 : message.substring(1,end_char).toInt();
}

String Communication::parseText(String message) {
  int start_char = message.indexOf(kStartOfTextChar) +1 ;
  int end_char = message.indexOf(kEndOfTextChar);
  return (end_char < 0 || end_char <= start_char) ? "" : message.substring(start_char,end_char);
}

String Communication::parseFooter(String message) {
  int start_char = message.indexOf(kEndOfTextChar) + 1;
  int end_char = message.length();
  return (end_char < 0 || end_char <= start_char) ? "" : message.substring(start_char,end_char);
}
