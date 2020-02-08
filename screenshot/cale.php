<?php
require './../vendor/autoload.php';
use Knp\Snappy\Image;

//screenshot_path: '/cale.php?u=[url]&w=[width]&h=[height]&c=[compressed]&b=[brightness]&d=[depth]&z=[zoom]'
$url = isset($_GET['u']) ? $_GET['u'] : null;

$compressed = (isset($_GET['c']) && $_GET['c']=='1') ?true:false;

$brightness = isset($_GET['b']) && ($_GET['b']>0 && $_GET['b']<200) ? $_GET['b'] : 100;

$width  = (isset($_GET['w']) && ($_GET['w']>1 && $_GET['b']<1201)) ? $_GET['w'] : null;

$height = (isset($_GET['h']) && ($_GET['h']>1 && $_GET['b']<1201)) ? $_GET['h'] : null;

$bitdepth   = (isset($_GET['d']) && ($_GET['d']>1 && $_GET['d']<5)) ? $_GET['d'] :1;

$zoomfactor = isset($_GET['z']) ? $_GET['z'] :'.8';

if (!is_numeric($width)||!is_numeric($height)) {
   $error = ['status'=>'error','message'=>'w / h should be an integer between 2 & 1200'];
   exit(json_encode($error));
}


$cacheEnabled = true;
$imageBasePath = './screenshots';
if (!$url) {
    exit('Url not defined');
}
// CACHE Validity
$validity = 1; // seconds * minutes

$start = microtime(true);
$host = parse_url($url, PHP_URL_HOST);
$path = urlencode(parse_url($url, PHP_URL_PATH));
if ($path == '') $path = 'index';
if (!file_exists($imageBasePath.'/'.$host)) {
    mkdir($imageBasePath.'/'.$host, 0777, true);
}

$filename = $imageBasePath.'/'.$host.'/'.$path.'.bmp';
if($cacheEnabled && file_exists($filename) && filectime($filename) > time() - $validity) {
    // cache is valid
    $file = file_get_contents($filename);
    exit($file);
}
//exit(print_r(Imagick::getVersion()));
$image = new Image();
$image->setBinary('/usr/local/bin/wkhtmltoimage');
$image->setOption('disable-smart-width', true);
$image->setOption('format', 'bmp');
$image->setOption('width', $width);
$image->setOption('height', $height);
$image->setOption('zoom', $zoomfactor);
// Set User agent (It does not work like this)
$image->setOption('custom-header', 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36');
// Last Safari
//$image->setOption('custom-header', 'User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9) AppleWebKit/537.71 (KHTML, like Gecko) Version/7.0 Safari/537.71');
$image->setOption('custom-header-propagation', true);
$image->setOption('disable-javascript', false);
// Initialize Imagick
$imageX = new Imagick();

try {
    $out = $image->getOutput($url);
} catch (RuntimeException $exception) {
    $draw = new ImagickDraw();
    $pixel = new ImagickPixel('white');
    $imageX->newImage($width, $height, $pixel);

    $imageX->annotateImage($draw, 10, 20, 0, $exception->getMessage());
    $imageX->setImageFormat('bmp');

    $imageX->setImageChannelDepth(Imagick::CHANNEL_GRAY, 4);
    $imageX->setImageType(Imagick::IMGTYPE_GRAYSCALE);
    exit($imageX);
}

$imageX->readImageBlob($out);
$saturation = 100;
$hue = 100;

$imageX->modulateImage($brightness, $saturation, $hue);
// This quantize returns a 1-bit image but does not work on 2.7 display
if ($bitdepth === 1) {
  $imageX->quantizeImage(2,     // Number of colors  
    Imagick::COLORSPACE_GRAY, // Colorspace
    8,                        // Depth tree  
    true,                     // Dither
    false);
}

// Commenting this lines drops a 24-bit BMP
  $imageX->posterizeimage(2, false);

try {
  $imageX->writeimage($filename);
} catch (Exception $exception) {
   $error = ['status'=>'error','message'=>'Failed on imagick->writeimage(file) message: '.$exception->getMessage()];
   exit(json_encode($error));
}

if ($compressed) {
  $fileread = fopen($filename, "r");
  $encoded = gzcompress(fread($fileread,filesize($filename)),9) or die("Can't zlib compress data: $php_errormsg");
  header('Content-Type:application/zlib');
  echo $encoded;

}
else {
  header('Content-Type:image/bmp');
  echo $imageX;
}

