## Server backend example 

This server backend is an example of how to achieve the only <b>one thing that the Firmware needs</b>:

A BMP image of 1 or 4 bits depth. Try first with 1 bit depth image and if for some reason does not work it can also receive a 4 bits BMP response.
We found out that for smaller displays our Firmware code to read 1 bit depth is working like expected, in that case, just return a 4-bit image. 

We used PHP as a server side language but it could be achieved with any language. The only thing that matters is that the response matches the expectations and the width/height size of your Eink display:

1. Response Content-Type:image/bmp
2. Image: Width / height in pixels should be equal to your display

## Library dependencies

To install the library dependencies we are using [Composer](https://getcomposer.org) just go in the command line to this directory and run:

    composer install

The index.php script generates a website screenshot of a given URL.
Make sure that the index.php Line:

    require './vendor/autoload.php';

Matches the **vendor** Folder location. 


## Expand it to match your Eink size

In the PHP we are receiving the **u** variable for the url that should be rendered and we added also a **eink** to represent the model:

    switch($eink_model) {
      case 'GxGDEW027C44':
       $displayWidth = 264;
       $displayHeight = 176;
       $zoomFactor = isset($_GET['z']) ? $_GET['z'] : '.6';
      break;
    }

This will return for a 2.7 " eink a 264x176 image. 
You can add your model to the case using this example and customize the width/height and zoom factor to get the desired output.
As default when no **eink** comes returns a 640*384 image. 
Feel free to expand this and your Eink display size. We prefer not to do it ourselves since we don't have all the models to test the output.

## Google calendar example

An example to render a dynamic google calendar is on the **g_calendar** directory along with instructions. 
