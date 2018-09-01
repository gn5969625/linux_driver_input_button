#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#define DRV_VERSION "1.0"
#define GPIO_NUM 1
typedef struct 
{
    int gpio;
    int irq;
    char name[20];
}gpio_data;

static gpio_data *gpio_bank;
static struct input_dev *button_input;

static irqreturn_t button_isr(int irq, void *dev_id) {
	gpio_data *my_gpio = (gpio_data *)dev_id;
	int pinval;
	pinval = (gpio_get_value_cansleep(my_gpio->gpio) ? 1 : 0) ^ 1;
	printk("%s enter, %s: gpio:%d, irq: %d, pinval:%d \n", __func__, my_gpio->name, my_gpio->gpio, my_gpio->irq,pinval);
	//input_report_key(button_input,KEY_L,!!pinval);
        input_event(button_input, EV_KEY, KEY_L, !!pinval);
	input_sync(button_input);
	return IRQ_HANDLED;
}

static int button_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    int irq_gpio = -1;
    int irq = -1;
    int ret = 0;
    int i = 0;
    char tmp[20];
    gpio_data *data = NULL;

    printk("%s enter\n",__func__);

    if(!dev->of_node) {
        dev_err(dev,"no of_node data");
	goto err1;
    }

    //init gpio
    data = devm_kmalloc(dev,sizeof(gpio_data)*GPIO_NUM,GFP_KERNEL);
    gpio_bank = data;
    if(!data) {
       dev_err(dev,"memory alloc error\n");
       goto err0;
    }

    for(i = 0;i < GPIO_NUM;i++) {
       sprintf(data[i].name , "mygpio%d",i);
       irq_gpio = of_get_named_gpio(dev->of_node,data[i].name,0);
       if(irq_gpio < 0) {
          dev_err(dev, "Looking up %s property in node %s failed %d\n",data[i].name, dev->of_node->full_name, irq_gpio);
          goto err0;
       }
      data[i].gpio = irq_gpio;
      irq = gpio_to_irq(irq_gpio);
      if(irq < 0) {
          dev_err(dev,"Unable to get irq number for GPIO %d, error %d\n",irq_gpio,irq);
          goto err0;
      }
      data[i].irq = irq;
      printk("%s: gpio: %d ---> irq (%d)\n", __func__, irq_gpio, irq);

      //ret = devm_request_irq(dev, irq, button_isr, IRQF_TRIGGER_FALLING, data[i].name,data+i);
      ret = devm_request_irq(dev, irq, button_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING , data[i].name,data+i);
      if(ret < 0) {
	dev_err(dev, "Unable to claim irq %d; error %d\n",irq, ret);
        goto err0;
      }
     
      sprintf(tmp,"%s_%s",data[i].name,"input");
      //request gpio to input direction
      //ret = gpio_request(data[i].gpio,data[i].name);
      //ret = gpio_direction_input(data[i].gpio);
      ret = gpio_request_one(data[i].gpio,GPIOF_DIR_IN,tmp);
    }
 
    //init input subsystem
    button_input = input_allocate_device();
    set_bit(EV_KEY,button_input->evbit);
    //set_bit(EV_REP,button_input->evbit);
    set_bit(KEY_L,button_input->keybit);
    ret = input_register_device(button_input);

    return 0;
err0:
    devm_kfree(dev,data);
err1:
    return -EINVAL;
}
static int button_remove(struct platform_device *pdev) {
        struct device *dev = &pdev->dev;
        int count = 0,i;
        printk("%s enter\n",__func__);
        for(count = 0; count < GPIO_NUM;count++) {
        	devm_free_irq(dev, (gpio_bank+count)->irq ,gpio_bank + count);
		gpio_free(gpio_bank[i].gpio);
	}
        devm_kfree(dev,gpio_bank);
        input_unregister_device(button_input);
	input_free_device(button_input);
	return 0;
}

static const struct of_device_id button_id[] = {
    { .compatible = "firefly,button", },
    {},
};
MODULE_DEVICE_TABLE(of, button_id);

static struct platform_driver button_driver = {
    .driver = {
        .name              = "firefly,button",
        .of_match_table    = of_match_ptr(button_id),
    },
    .probe                 = button_probe,
    .remove                = button_remove,
};

static int __init button_init(void)
{
	int ret;
	ret = platform_driver_register(&button_driver);
	if(ret)
		printk(KERN_ERR "button demo: register failed: %d\n", ret);
	return ret;
}

static void __exit button_exit(void)
{
	platform_driver_unregister(&button_driver);
}

module_init(button_init);
module_exit(button_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin.Shen");
MODULE_DESCRIPTION("A button-input subsystem driver");
MODULE_VERSION(DRV_VERSION);
