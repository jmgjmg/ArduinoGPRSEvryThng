<article class="markdown-body entry-content" itemprop="mainContentOfPage"><h1>ArduinoGPRSEvrythng: Uploading Data to Evrythng from Arduino with GPRS shield</h1>
<p>This is a sample Arduino application that uses SeeedStudio NFC and GPRS shields. It has been tested with Arduino Uno (v1.0) but it should also work with other boards.</p>

<p>The setup method initialises the NFC and GPRS shields. Then the loop method checks every one second for the presence of one RFID (MIFARE) tag. When one tag is detected, its Id is pushed to evrythng using a GPRS connection.</p>

<p>The code defines two LEDs to provide basic visual feedback. This can be removed or updated as required</p>

<h2>NFC Shield</h2>
<p>The NFC shield must be plugged directly into the Arduino Uno (the shield uses the ICSP PINs so it will not work if plugged into the GPRS shield that does not expose ICSP PINs).</p>
<p>The code uses the NFC library from SeeedStudio downloadable from <a href="http://www.seeedstudio.com/wiki/index.php?title=NFC_Shield">SeeedStudio Wiki</a>. This library needs to be updated with a new method <code>PN532::RFConfiguration</code> as explained <a href="#rfconfig">below</a>. This method updates the timeout used by the NFC chip (NXP PN532) to stop searching for new tags.</p>

<pre style="margin: 0 20px 0;"><code><a name="rfconfig"></a>/*
//WARNING: This function must be added to the PN532 library downloaded from SeeedStudio
void PN532::RFConfiguration(uint8_t mxRtyPassiveActivation) {
    pn532_packetbuffer[0] = PN532_RFCONFIGURATION;
    pn532_packetbuffer[1] = PN532_MAX_RETRIES;
    pn532_packetbuffer[2] = 0xFF; // default MxRtyATR
    pn532_packetbuffer[3] = 0x01; // default MxRtyPSL
pn532_packetbuffer[4] = mxRtyPassiveActivation;

sendCommandCheckAck(pn532_packetbuffer, 5);
    // ignore response!
}
*/
</code></pre>

<h2>GPRS Shield</h2>

<p>The PIN of the SIM card must be disabled so no PIN validation is performed.</p>
<p>Update the mobile settings (APN, UserName, Password) with the data of your MNO.</p>  
<p>No library is provided by SeeedStudio for managing the GPRS shield so all the AT commands 
  are sent as byte strings over the serial interface. This is a bit tricky specially when 
  the handling of the responses must work in parallel to the reading of NFC tags.
  Please refer to <a href="http://garden.seeedstudio.com/images/a/a8/SIM900_AT_Command_Manual_V1.03.pdf">the documentation 
  of SIM900 module</a> for a detailed description of the different AT commands. The new GPRS shield and library being
  developed by Arduino and Telefonica should simplify the code considerably.</p>
  
<p>Note that the http response data received from evrythng server is not fully 
  read/parsed. Further work would be required to process the whole repsonse (currently only the 
  first 63 bytes of the response are read). I think that this is due to an internal buffer
  in the SIM900 GPRS module.</p>
<h2>Evrythng Setup</h2>
You have to retrieve your authentication token from https://evrythng.net/settings/tokens and update its value in your Arduino code:
<pre><code style="margin: 0 20px 5px;">GPRS_Serial.println("X-Evrythng-Token: ...yourAPITokenHere...");</code></pre>
You must also crate a new thng in evrytng with a property called ReadTag. Once created, you have to update its thngId value in your Arduino code:
<pre><code style="margin: 0 20px 5px;">GPRS_Serial.println("PUT http://evrythng.net/thngs/...yourThingIdHere.../properties/ReadTag HTTP/1.1");</code></pre>
<h2>License</h2>
<p>Copyright 2012 Javier Montaner</p>

<p>The ArduinoGPRSEvrythng software is licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at <a href="http://www.apache.org/licenses/LICENSE-2.0">http://www.apache.org/licenses/LICENSE-2.0</a></p>

<p>Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.</p>
</article>