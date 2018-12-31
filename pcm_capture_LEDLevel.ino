/*
 *  pcm_capture_LEDLevel.ino - PCM capture and peak display example application
 *  Copyright 2018 Sony Semiconductor Solutions Corporation & Chuck Swiger
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Audio.h>      // Sony Audio library
#include <Adafruit_DotStar.h>
#include <SPI.h>         // Hardware SPI for fast DotStars

#define NUMPIXELS 144 // Number of LEDs in strip

AudioClass *theAudio;   // Singleton? 
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DOTSTAR_BRG);

static const int32_t buffer_size = 512;      // 256 samples per call at 16K, 2 bytes per, 1 ch mono
static char          s_buffer[buffer_size];

bool ErrEnd = false;

/**
 * @brief Audio attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */

void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");
  
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
    {
      ErrEnd = true;
   }
}

/**
 *  @brief Setup audio device to capture PCM stream
 *
 *  Select input device as microphone <br>
 *  Set PCM capture sapling rate parameters to 16 kb/s <br>
 *  Set channel number 1 to capture audio from 1 microphone mono <br>
 *  System directory "/mnt/sd0/BIN" will be searched for PCM codec
 */
void setup()
{
  theAudio = AudioClass::getInstance();

  theAudio->begin(audio_attention_cb);

  // init the LED strip
  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP

  puts("initialization Audio Library");

  /* Select input device as microphone with max gain +21db for electret */
  theAudio->setRecorderMode(AS_SETRECDR_STS_INPUTDEVICE_MIC,210);

  /*
   * Set PCM capture sapling rate parameters to 16 kb/s. Set channel MONO
   * Search for PCM codec in "/mnt/sd0/BIN" directory
   */
  theAudio->initRecorder(AS_CODECTYPE_PCM, "/mnt/sd0/BIN", AS_SAMPLINGRATE_16000, AS_CHANNEL_MONO);
  puts("Init Recorder!");

  puts("Rec!");
  theAudio->startRecorder();
}

/**
 * @brief Audio signal process for your application
 */


void signal_process(uint32_t size)    // this does not automatically get buffer_size each time! Usually 512 for 256 samples
{
  uint32_t ON_color = 0x808080;      // 'On' color - mid-white is darned bright!
  uint32_t OFF_color = 0x000000;      // 'Off' color, off
  signed int sigmax = 0;
  uint16_t idx;
  union Combine
  {
      short target;
      char dest[ sizeof( short ) ];
  };
  Combine cc;
  for(idx = 0; idx<size; idx+=2) {
    cc.dest[0] = s_buffer[idx];
    cc.dest[1] = s_buffer[idx+1];
    if ( cc.target > sigmax ) {             // Look for the largest one in this batch of 256, only looking for the postive samples
      sigmax = cc.target; 
    }
  }
  //printf("%d %d\n",size,sigmax);
  // map sigmax 0 to 65535  to  leds 0 to 143
  uint8_t maxled = map(sigmax,0,32767,0,144);
  for(idx=0; idx<maxled; idx++)
    strip.setPixelColor(idx, ON_color); // 'On' pixels up to the remapped sigmax
  for(idx=maxled; idx<NUMPIXELS; idx++)
    strip.setPixelColor(idx, OFF_color);  // 'Off' pixel the rest of the strip
  strip.show();                           // update strip
}

/**
 * @brief Execute frames for FIFO empty
 */

void execute_frames()
{
  uint32_t read_size = 0;
  do
    {
      err_t err = execute_aframe(&read_size);
      if ((err != AUDIOLIB_ECODE_OK)
       && (err != AUDIOLIB_ECODE_INSUFFICIENT_BUFFER_AREA))
        {
          break;
        }
    }
  while (read_size > 0);
}

/**
 * @brief Execute one frame
 */

err_t execute_aframe(uint32_t* size)
{
  err_t err = theAudio->readFrames(s_buffer, buffer_size, size);

  if(((err == AUDIOLIB_ECODE_OK) || (err == AUDIOLIB_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0)) 
    {
      signal_process(*size);
    }

  return err;
}

/**
 * @brief Capture frames of PCM data into buffer
 */
void loop() {

  static int32_t total_size = 0;
  uint32_t read_size =0;

  /* Execute audio data */
  err_t err = execute_aframe(&read_size);
  if (err != AUDIOLIB_ECODE_OK && err != AUDIOLIB_ECODE_INSUFFICIENT_BUFFER_AREA)
    {
      theAudio->stopRecorder();
      goto exitRecording;
    }
  else if (read_size>0)
    {
      total_size += read_size;
    }


  /* Never Stop Recording! */

  if (ErrEnd)
    {
      printf("Error End\n");
      theAudio->stopRecorder();
      goto exitRecording;
    }

  return;

exitRecording:

  theAudio->setReadyMode();
  theAudio->end();
  
  puts("End Recording");
  exit(1);

}
