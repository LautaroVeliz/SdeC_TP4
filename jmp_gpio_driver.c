#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrada Esteban Nicolas - Bustos Figueroa Maria Cecilia -"
              "Veliz Lautaro Benjamin");
MODULE_DESCRIPTION("Raspberry GPIO driver");

static dev_t devID;            // Identifica al dispositivo (MAJOR - MINOR)
static struct cdev charDev;    // Estructura de datos interna al kernel que
                               // representa un dispositivo de caracteres.
                               // Intermente almacena el dev_t que identifica
                               // al dispositivo
static struct class *devClass; // Identifica la clase de dispositivo que se va a crear

#define sig1 0
#define sig2 1
static unsigned int signalSelec = sig1; // Identificador de la señal que 
                                        // actualmente se está leyendo
static char senalString[] = "0000";     // String de bits de lectura

static struct gpio s1[] = {             // {GPIO#, Modo, señal#_boton# }
    { 12, GPIOF_IN, "s1_b4" },	        // señal 1, botón 4
	{ 16, GPIOF_IN, "s1_b3" },
    { 20, GPIOF_IN, "s1_b2" },
    { 21, GPIOF_IN, "s1_b1" }	
    };

static struct gpio s2[] = {
    { 18, GPIOF_IN, "s2_b4" },	        // señal 2, botón 4
	{ 23, GPIOF_IN, "s2_b3" },  
    { 24, GPIOF_IN, "s2_b2" },
    { 25, GPIOF_IN, "s2_b1" }};

static struct gpio *pSL = s1;           // Puntero apuntando a la estructura
                                        // de gpio de la señal actual

// Es llamada cuando se abre el archivo a nivel de usuario
// Vincula fd de nivel de usuario con el inodo de nivel de kernel
static int my_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Raspberry GPIO driver: open()\n");
    return 0;
}
// Es llamada cuando se cierra el archivo a nivel de usuario
static int my_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Raspberry GPIO driver: close()\n");
    return 0;
}

// Funcion de escritura del driver
static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    // Al escribir se cambia de señal
    if (signalSelec == sig1)
    {
        signalSelec = sig2;
    }
    else
    {
        signalSelec = sig1;
    }
    printk(KERN_INFO "Se ha seleccionado la señal %d para sensar\n", signalSelec);
    return len;
}

// Funcion de lectura del driver
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    int nr_bytes, i;
    char valorPin;

    if (signalSelec == sig1)
        pSL = s1;
    else
        pSL = s2;

    // Leer pines de la señal seleccionada
    for (i = 0; i < 4; i++)
    {
        valorPin = gpio_get_value(pSL[i].gpio);
        printk(KERN_INFO "%d", valorPin);
        if (valorPin)
            senalString[i] = '1';
        else
            senalString[i] = '0';
    }
    senalString[5] = '\0';
    nr_bytes = strlen(senalString);

    if ((*off) > 0) // ¿Archivo vacío?
        return 0;

    if (len < nr_bytes) // Si el buffer es más chico que el string
        return -ENOSPC;

    // Transfiere data desde el espacio de kernel al espacio de usuario
    if (copy_to_user(buf, senalString, nr_bytes))
        return -EINVAL;

    (*off) += len; // Actualizar el puntero del archivo escrito

    return nr_bytes;
}

static const struct file_operations dev_entry_fops =
    {
        .owner = THIS_MODULE,
        .read = gpio_read,
        .write = gpio_write,
        .open = my_open,
        .release = my_close
    };

// Esta función se llama cuando se carga el módulo con el comando insmod
// Realiza las inicializaciones preliminares del módulo para ser utilizado
int raspGPIODr_init(void)
{
    int ret = 0;
    struct device *dev_ret;

    printk(KERN_INFO "Raspberry GPIO driver... IS UP!!!\n");

    // Reserva de los números major y minor asociados al dispositivo
    // major: identifica al manejador
    // minor: identifica al dispositivo concreto entre los que gestiona ese manejador
    // dev_t devID: almacenará el identificador del dispositivo dentro del núcleo.
    //              Internamente, está compuesto por los valores major y minor asociados
    //              a ese dispositivo.
    // El major lo elige el SO
    // 0: primer minor que solicitamos
    // 1: cantidad de números minor que se quieren reservar
    // catangaF1: nombre del dispositivo
    if ((ret = alloc_chrdev_region(&devID, 0, 1, "RaspberryGPIODriver")) < 0)
    {
        printk(KERN_INFO "Error en alloc_chrdev_region()\n");
        return ret;
    }

    // Crear una clase para el dispositivo gestionado por el manejador
    // catangaF1Class: nombre de la clase
    // THIS_MODULE: driver que gestiona este tipo de dispositivos
    if (IS_ERR(devClass = class_create(THIS_MODULE, "RaspberryGPIODriverClass")))
    {
        printk(KERN_INFO "Error en class_create()\n");
        unregister_chrdev_region(devID, 1); // En caso de error, se liberan los major y minors
        return PTR_ERR(devClass);
    }

    // crea un dispositivo de la clase catangaF1Class y lo registra con sysfs con nombre miCatangaF1Sysfs
    // devClass: clase de dispositivo que se va a crear
    // devID: mayor y minor que identifica el dispositivo concreto
    // miCatangaF1Sysfs: nombre del fichero del dispositivo con el que figurará en /dev/
    // A partir de este momento, se habrá creado el fichero /dev/seq y las llamadas de apertura,
    // lectura, escritura,
    // cierre, etc. sobre ese fichero especial son redirigidas por el SO a las funciones de acceso
    // correspondientes exportadas por el manejador del dispositivo.
    if (IS_ERR(dev_ret = device_create(devClass, NULL, devID, NULL, "raspGPIODr")))
    {
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        return PTR_ERR(dev_ret);
    }

    // Inicializar la estructura charDev que representa un dispositivo de caracteres
    // pugs_fops: conjunto de funciones de acceso que proporciona el dispositivo
    cdev_init(&charDev, &dev_entry_fops);
    if ((ret = cdev_add(&charDev, devID, 1)) < 0)
    {
        device_destroy(devClass, devID);
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        return ret;
    }

    // Se reservan los pines a utilizar por la Señal 1
    ret = gpio_request_array(s1, ARRAY_SIZE(s1));
    if (ret)
    { // Error
        printk(KERN_ERR "Error en gpio_request_array para s1: %d\n", ret);
        device_destroy(devClass, devID);
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        gpio_free_array(s1, ARRAY_SIZE(s1));
    }

    // Se reservan los pines a utilizar por la Señal 2
    ret = gpio_request_array(s2, ARRAY_SIZE(s2));
    if (ret)
    { // Error
        printk(KERN_ERR "Error en gpio_request_array para s1: %d\n", ret);
        device_destroy(devClass, devID);
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        gpio_free_array(s1, ARRAY_SIZE(s1));
        gpio_free_array(s2, ARRAY_SIZE(s1));
    }
    return 0;
}

// Esta función se llama cuando se descarga el módulo del kernel con el comando rmmod
// Realiza las operaciones inversas realizadas por init_module al cargarse el módulo
void raspGPIODr_exit(void)
{
    device_destroy(devClass, devID);
    class_destroy(devClass);
    unregister_chrdev_region(devID, 1);
    cdev_del(&charDev);
    printk(KERN_INFO "Raspberry GPIO driver... is down...:(\n");
    gpio_free_array(s1, ARRAY_SIZE(s1));
    gpio_free_array(s2, ARRAY_SIZE(s2));
}

module_init(raspGPIODr_init); // Renombra init_module
module_exit(raspGPIODr_exit); // Renombra cleanup_module