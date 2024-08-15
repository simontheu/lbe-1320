//#include <iostream>
//using namespace std;

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/* Unix */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>

#include <time.h>

//Leo LBE-1320 defines, dont need anything external this time
#define VID_LBE		0x1dd2

#define PID_LBE_1320	0x2301

//Report Layout
//Byte 0 = 0 FIXED
//Byte 1 = 1 FIXED

//Amplitude Range 0-510
//Byte 2 = 0-255  Amplitude LSBs 
//Byte 3 = 0-1  Amplitude MSB, only the lowest bit is used
//Byte 4 = 0-1 Polarity, 0-Off, 1-Normal, 2-Inverted
//Byte 5 = 0 - Reserved
//Byte 6 = Resrverd 0x02 (Windows GUI Calibration MSBs)
//Byte 7 = Reserved 0x58 (Windows GUI Calibration LSBs)

/*
 * Ugly hack to work around failing compilation on systems that don't
 * yet populate new version of hidraw.h to userspace.
 */
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif

#define HIDIOCGRAWNAME(len)     _IOC(_IOC_READ, 'H', 0x04, len)


int processCommandLineArguments(int argc, char **argv, int *amp, int *pol);

int main(int argc, char **argv)
{
      printf("Leo Bodnar LBE-1310 Fast Risetime Pulser\n");
      
      int fd;
      int i, res, desc_size = 0;
      u_int8_t buf[60];
      uint32_t current_f;

      struct hidraw_devinfo info;

      int prev_amp;
      int prev_pol;
      int prev_cal;
      int fact_cal;
    
//      GPSSettings *currentSettings = new GPSSettings;

   /* Open the Device with non-blocking reads. In real life,
      don't use a hard coded path; use libudev instead. 
   */
      if (argc == 1)
      {
	    printf("Usage: lbe-1320 /dev/hidraw??\n\n");
            printf("      --amp:  0-510 (Maps to 50mV - 1645mV)\n");
            printf("      --pol:  0=Off, 1=Normal, 2=Inverted,\n\n");
            return -1;
      }

      printf("Opening device %s\n", argv[1]);

      fd = open(argv[1], O_RDWR|O_NONBLOCK);

      if (fd < 0) 
      {
            perror("    Unable to open device");
            return 1;
      }

      //Device connected, setup report structs
      memset(&info, 0x0, sizeof(info));

      // Get Raw Info
      res = ioctl(fd, HIDIOCGRAWINFO, &info);
      
      if (res < 0) 
      {
            perror("HIDIOCGRAWINFO");
      } 
      else
      {
            if (info.vendor != VID_LBE || (info.product != PID_LBE_1320)) {
                printf("    Not a valid LBE-1320 Device\n\n");
                  printf("    Device Info:\n");
                  printf("        vendor: 0x%04hx\n", info.vendor);
                  printf("        product: 0x%04hx\n", info.product);
                  return -1;//Device not valid
            }
      }

      /* Get Raw Name */
      res = ioctl(fd, HIDIOCGRAWNAME(256), buf);

      if (res < 0) {
            perror("HIDIOCGRAWNAME");
      }
      else {
            printf("Connected To: %s\n\n", buf);
      }

      /* Get Feature */
      buf[0] = 0x0; /* Report Number */
      res = ioctl(fd, HIDIOCGFEATURE(256), buf);

      if (res < 0) {
            perror("HIDIOCGFEATURE");
      } else {
	      printf("  Status:\n");



          if (buf[1] == 0xff) {
            prev_amp = buf[1] * 2;
          } else {
            prev_amp = (buf[1] * 2) + (buf[2] & 0x01);
          }
          printf("    Amplitude: %i\n",prev_amp);
          
          //Output/Polarity
          if (buf[3] > 0x02) {
            prev_pol = 1;
          } else {
            prev_pol = buf[3];
          }
          printf("    Polarity: %i ",prev_pol);
          switch (prev_pol) {
            case 0:
                printf("(Off)\n");
                break;
            case 1:
                printf("(Normal)\n");
                break;
            case 2:
                printf("(Inverted)\n");
                break;
          }
          
          
          //Cal
          prev_cal = (buf[5] * 0x100) + buf[6];
          printf("    Cal: %i\n",prev_cal);
          
          //Factory Cal
          fact_cal = (buf[7] * 0x100) + buf[8];// / 10000
          printf("    Factory Cal: %i\n",fact_cal);
          printf("\n\n");
          /**/
            
      }

      /* Get Raw Name */
      res = ioctl(fd, HIDIOCGRAWNAME(256), buf);

      if (res < 0) {
            perror("HIDIOCGRAWNAME");
      }
      else {

	//Get CLI values as vars
	int amp = -1;//previous amp
	int pol = -1;//previous pol
    int cal = -1;//previouse cal

	processCommandLineArguments(argc, argv,  &amp, &pol);
    
    
    int changed = 0;

    if (amp != -1 && amp != prev_amp) {
        printf("  Changed amp to: %i\n",amp);
        changed = 1;
    } else {
        amp = prev_amp;
    }
    
    if (pol != -1 && pol != prev_pol) {
        printf("  Changed pol to: %i\n",pol);
        changed = 1;
    } else {
        pol = prev_pol;
    }
    
    if (changed) {
        buf[0] = 0;
        buf[1] = 1;//Feature 1

        //Amplitude
        buf[2] = (amp >> 1) & 0xff;//LSB
        buf[3] = (amp & 0x1);//MSB
        
        //Output Mode/Polarity
        buf[4] = (pol < 3 ? pol : 1);

        //Reserved
        buf[5] = 0;

        //Cal (Windows GUI Only)
        buf[6] = (prev_cal >> 8) & 0xff;
        buf[7] = prev_cal & 0xff;
        res = ioctl(fd, HIDIOCSFEATURE(60), buf);
        if (res < 0) {
            perror("HIDIOCSFEATURE");
        } else {
            printf("    Made changes successfully\n");
        }
        
        for (int i=0; i< 8; i++ ) {
            //printf("%i:%02x\n", i, buf[i]);
        }

        printf("\n\n");
    } else {
        printf("  No changes made\n\n\n");
    }

        /*
	if (new_f != 0xffffffff && new_f != current_f) {
	    //Set Frequency
	    printf ("    Setting Frequecy: %i\n", new_f);
	    
	    buf[0] = (save == 1 ? 4 : 3);//4 Save, 3 dont save
	    buf[1] = (new_f >>  0) & 0xff;
	    buf[2] = (new_f >>  8) & 0xff;
	    buf[3] = (new_f >> 16) & 0xff;
     	    buf[4] = (new_f >> 24) & 0xff;

            res = ioctl(fd, HIDIOCSFEATURE(60), buf);
            if (res < 0) perror("HIDIOCSFEATURE");
            changed = 1;
	}
	if (enable != -1) {
	    buf[0] = 1;
	    buf[1] = enable & 0x01;
	    printf ("    Enable State :%i\n", enable);

            res = ioctl(fd, HIDIOCSFEATURE(60), buf);
            if (res < 0) perror("HIDIOCSFEATURE");
            changed = 1;
	}
	if (blink != -1) {
	    buf[0] = 2;
	    printf ("    Blink LED\n");

            res = ioctl(fd, HIDIOCSFEATURE(60), buf);
            if (res < 0) perror("HIDIOCSFEATURE");
            changed = 1;
	}
	if (!changed) {
	    printf("    No changes made\n");
	}
    */
      }
      
      close(fd);

      return 0;
}


int processCommandLineArguments(int argc, char **argv, int *amp, int *pol)
{
    int c;
    
    while (1)
    {
        static struct option long_options[] =
        {
                /* These options set a flag. */
                {"blink1", no_argument, 0, 0},
                /* These options don’t set a flag.
                    We distinguish them by their indices. */
                {"amp",    required_argument, 0, 'a'},
                {"pol",     required_argument, 0, 'b'},
                {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "abc:d:f:",
                    long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
        break;

        switch (c)
        {
            case 'a'://f1
                *amp = atoi(optarg);
                break;

            case 'b'://f1_nosave
                *pol = atoi(optarg);
                break;

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                abort ();
        }
    }
    return 0;
}

