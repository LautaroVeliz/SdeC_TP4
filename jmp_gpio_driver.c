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

static dev_t devID;           // Identifica al dispositivo (MAJOR - MINOR)
static struct cdev charDev;   // Estructura de datos interna al kernel que 
                              // representa un dispositivo de caracteres.
                              // Intermente almacena el dev_t que identifica 
                              // al dispositivo
static struct class *devClass;// Identifica la clase de dispositivo que se va a crear

// ===========================================================================
#define s1 17
#define s2 18

static unsigned int signalSelec = s1; // Identificador de la señal que actualmente se está leyendo
// ===========================================================================
// Es llamada cuando se abre el archivo a nivel de usuario
// Vincula fd de nivel de usuario con el inodo de nivel de kernel
static int my_open(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Raspberry GPIO driver: open()\n");
    return 0;
}
//----------------------------------------------------------------------------
// Es llamada cuando se cierra el archivo a nivel de usuario
static int my_close(struct inode *i, struct file *f)
{
    printk(KERN_INFO "Raspberry GPIO driver: close()\n");
    return 0;
}
// ===========================================================================
static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) 
{
    // Al presionar una tecla se cambia de señal
    if (signalSelec == s1)
    {
        signalSelec = s2;
    }
    else
    {
        signalSelec = s1;
    }

    printk(KERN_INFO "Se ha seleccionado la señal %d para sensar\n", signalSelec);
    return len;
}
//----------------------------------------------------------------------------
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off) 
{
    int nr_bytes;
    char valorPin;

    // Leer pine de la señal seleccionada
    valorPin = gpio_get_value(signalSelec);
    printk(KERN_INFO "%d", valorPin);
    if (valorPin)
        valorPin = '1';
    else
        valorPin = '0';

    nr_bytes = sizeof(valorPin);

    if ((*off) > 0) // ¿Archivo vacío?
        return 0;

    if (len < nr_bytes) // Si el buffer es más chico que el string
        return -ENOSPC;

    // Transfiere data desde el espacio de kernel al espacio de usuario
    if (copy_to_user(buf, &valorPin, nr_bytes))
        return -EINVAL;

    (*off) += len; // Actualizar el puntero del archivo escrito

    return nr_bytes; 
}
// ===========================================================================
static const struct file_operations dev_entry_fops = 
{
  .owner = THIS_MODULE,
  .read = gpio_read,
  .write = gpio_write, 
  .open = my_open,
  .release = my_close   
}; 
// ===========================================================================
// Esta función se llama cuando se carga el módulo con el comando insmod
// Realiza las inicializaciones preliminares del módulo para ser utilizado
int raspGPIODr_init( void )
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

    if (gpio_request_one(s1, GPIOF_IN, NULL) < 0)
    {
        printk(KERN_ERR "Error requesting GPIO %d\n", s1);
        device_destroy(devClass, devID);
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        cdev_del(&charDev);
        return -ENODEV;
    }

    if (gpio_request_one(s2, GPIOF_IN, NULL) < 0)
    {
        printk(KERN_ERR "Error requesting GPIO %d\n", s2);
        device_destroy(devClass, devID);
        class_destroy(devClass);
        unregister_chrdev_region(devID, 1);
        cdev_del(&charDev);
        gpio_free(s1);
        return -ENODEV;
    }

    return 0;
}
//----------------------------------------------------------------------------
// Esta función se llama cuando se descarga el módulo del kernel con el comando rmmod
// Realiza las operaciones inversas realizadas por init_module al cargarse el módulo
void raspGPIODr_exit( void )
{
    device_destroy(devClass, devID);
    class_destroy(devClass);
    unregister_chrdev_region(devID, 1);
    cdev_del(&charDev);
    printk(KERN_INFO "Raspberry GPIO driver... is down...:(\n");
    gpio_free(s1);
    gpio_free(s2);
}
//==================================================================================
module_init( raspGPIODr_init ); // Renombra init_module
module_exit( raspGPIODr_exit ); // Renombra cleanup_module
                                //==================================================================================
