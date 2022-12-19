#!/usr/bin/perl -w

package exif;

use strict;

BEGIN {
  use Exporter();
  use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

  $VERSION = 0.2;

  @ISA = qw(Exporter);
  @EXPORT = qw(&dump_jpeg);
  %EXPORT_TAGS = ();		# ???

  @EXPORT_OK = qw($newsubfiletype
		  $imagewidth
		  $imagelength
		  $bitspersample
		  $compression
		  $photometricinterpretation
		  $fillorder
		  $documentname
		  $imagedescription
		  $make
		  $model
		  $stripoffsets
		  $orientation
		  $samplesperpixel
		  $rowsperstrip
		  $stripbytecounts
		  $xresolution
		  $yresolution
		  $planarconfiguration
		  $resolutionunit
		  $transferfunction
		  $software
		  $datetime
		  $artist
		  $whitepoint
		  $primarychromaticities
		  $transferrange
		  $jpegproc
		  $jpeginterchangeformat
		  $jpeginterchangeformatlength
		  $ycbcrcoefficients
		  $ycbcrsubsampling
		  $ycbcrpositioning
		  $referenceblackwhite
		  $cfarepeatpatterndim
		  $cfapattern
		  $batterylevel
		  $copyright
		  $exposuretime
		  $fnumber
		  $iptc_naa
		  $exifoffset
		  $intercolorprofile
		  $exposureprogram
		  $spectralsensitivity
		  $gpsinfo
		  $isospeedratings
		  $oecf
		  $exifversion
		  $datetimeoriginal
		  $datetimedigitized
		  $componentsconfiguration
		  $compressedbitsperpixel
		  $shutterspeedvalue
		  $aperturevalue
		  $brightnessvalue
		  $exposurebiasvalue
		  $maxaperturevalue
		  $subjectdistance
		  $meteringmode
		  $lightsource
		  $flash
		  $focallength
		  $makernote
		  $usercomment
		  $subsectime
		  $subsectimeoriginal
		  $subsectimedigitized
		  $flashpixversion
		  $colorspace
		  $exifimagewidth
		  $exifimagelength
		  $interoperabilityoffset
		  $flashenergy
		  $spatialfrequencyresponse
		  $focalplanexresolution
		  $focalplaneyresolution
		  $focalplaneresolutionunit
		  $subjectlocation
		  $exposureindex
		  $sensingmethod
		  $filesource
		  $scenetype);
}

use vars @EXPORT_OK;

my $long = "";
my $short = "";
my $rational = "";
my $dirent = "";

$newsubfiletype			= 0x00fe;
$imagewidth			= 0x0100;
$imagelength			= 0x0101;
$bitspersample			= 0x0102;
$compression			= 0x0103;
$photometricinterpretation	= 0x0106;
$fillorder			= 0x010a;
$documentname			= 0x010d;
$imagedescription		= 0x010e;
$make				= 0x010f;
$model				= 0x0110;
$stripoffsets			= 0x0111;
$orientation			= 0x0112;
$samplesperpixel		= 0x0115;
$rowsperstrip			= 0x0116;
$stripbytecounts		= 0x0117;
$xresolution			= 0x011a;
$yresolution			= 0x011b;
$planarconfiguration		= 0x011c;
$resolutionunit			= 0x0128;
$transferfunction		= 0x012d;
$software			= 0x0131;
$datetime			= 0x0132;
$artist				= 0x013b;
$whitepoint			= 0x013e;
$primarychromaticities		= 0x013f;
$transferrange			= 0x0156;
$jpegproc			= 0x0200;
$jpeginterchangeformat		= 0x0201;
$jpeginterchangeformatlength	= 0x0202;
$ycbcrcoefficients		= 0x0211;
$ycbcrsubsampling		= 0x0212;
$ycbcrpositioning		= 0x0213;
$referenceblackwhite		= 0x0214;
$cfarepeatpatterndim		= 0x828d;
$cfapattern			= 0x828e;
$batterylevel			= 0x828f;
$copyright			= 0x8298;
$exposuretime			= 0x829a;
$fnumber			= 0x829d;
$iptc_naa			= 0x83bb;
$exifoffset			= 0x8769;
$intercolorprofile		= 0x8773;
$exposureprogram		= 0x8822;
$spectralsensitivity		= 0x8824;
$gpsinfo			= 0x8825;
$isospeedratings		= 0x8827;
$oecf				= 0x8828;
$exifversion			= 0x9000;
$datetimeoriginal		= 0x9003;
$datetimedigitized		= 0x9004;
$componentsconfiguration	= 0x9101;
$compressedbitsperpixel		= 0x9102;
$shutterspeedvalue		= 0x9201;
$aperturevalue			= 0x9202;
$brightnessvalue		= 0x9203;
$exposurebiasvalue		= 0x9204;
$maxaperturevalue		= 0x9205;
$subjectdistance		= 0x9206;
$meteringmode			= 0x9207;
$lightsource			= 0x9208;
$flash				= 0x9209;
$focallength			= 0x920a;
$makernote			= 0x927c;
$usercomment			= 0x9286;
$subsectime			= 0x9290;
$subsectimeoriginal		= 0x9291;
$subsectimedigitized		= 0x9292;
$flashpixversion		= 0xa000;
$colorspace			= 0xa001;
$exifimagewidth			= 0xa002;
$exifimagelength		= 0xa003;
$interoperabilityoffset		= 0xA005;
$flashenergy			= 0xa20b;
$spatialfrequencyresponse	= 0xa20c;
$focalplanexresolution		= 0xa20e;
$focalplaneyresolution		= 0xa20f;
$focalplaneresolutionunit	= 0xa210;
$subjectlocation		= 0xa214;
$exposureindex			= 0xa215;
$sensingmethod			= 0xa217;
$filesource			= 0xa300;
$scenetype			= 0xa301;

my %tag_info;

$tag_info{$newsubfiletype}{desc}		= "NewSubFileType";
$tag_info{$imagewidth}{desc}			= "ImageWidth";
$tag_info{$imagelength}{desc}			= "ImageLength";
$tag_info{$bitspersample}{desc}			= "BitsPerSample";
$tag_info{$compression}{desc}			= "Compression";
$tag_info{$photometricinterpretation}{desc}	= "PhotometricInterpretation";
$tag_info{$fillorder}{desc}			= "FillOrder";
$tag_info{$documentname}{desc}			= "DocumentName";
$tag_info{$imagedescription}{desc}		= "ImageDescription";
$tag_info{$make}{desc}				= "Make";
$tag_info{$model}{desc}				= "Model";
$tag_info{$stripoffsets}{desc}			= "StripOffsets";
$tag_info{$orientation}{desc}			= "Orientation";
$tag_info{$samplesperpixel}{desc}		= "SamplesPerPixel";
$tag_info{$rowsperstrip}{desc}			= "RowsPerStrip";
$tag_info{$stripbytecounts}{desc}		= "StripByteCounts";
$tag_info{$xresolution}{desc}			= "XResolution";
$tag_info{$yresolution}{desc}			= "YResolution";
$tag_info{$planarconfiguration}{desc}		= "PlanarConfiguration";
$tag_info{$resolutionunit}{desc}		= "ResolutionUnit";
$tag_info{$transferfunction}{desc}		= "TransferFunction";
$tag_info{$software}{desc}			= "Software";
$tag_info{$datetime}{desc}			= "DateTime";
$tag_info{$artist}{desc}			= "Artist";
$tag_info{$whitepoint}{desc}			= "WhitePoint";
$tag_info{$primarychromaticities}{desc}		= "PrimaryChromaticities";
$tag_info{$transferrange}{desc}			= "TransferRange";
$tag_info{$jpegproc}{desc}			= "JPEGProc";
$tag_info{$jpeginterchangeformat}{desc}		= "JPEGInterchangeFormat";
$tag_info{$jpeginterchangeformatlength}{desc}  = "JPEGInterchangeFormatLength";
$tag_info{$ycbcrcoefficients}{desc}		= "YCbCrCoefficients";
$tag_info{$ycbcrsubsampling}{desc}		= "YCbCrSubSampling";
$tag_info{$ycbcrpositioning}{desc}		= "YCbCrPositioning";
$tag_info{$referenceblackwhite}{desc}		= "ReferenceBlackWhite";
$tag_info{$cfarepeatpatterndim}{desc}		= "CFARepeatPatternDim";
$tag_info{$cfapattern}{desc}			= "CFAPattern";
$tag_info{$batterylevel}{desc}			= "BatteryLevel";
$tag_info{$copyright}{desc}			= "Copyright";
$tag_info{$exposuretime}{desc}			= "ExposureTime";
$tag_info{$fnumber}{desc}			= "FNumber";
$tag_info{$iptc_naa}{desc}			= "IPTC/NAA";
$tag_info{$exifoffset}{desc}			= "ExifOffset";
$tag_info{$intercolorprofile}{desc}		= "InterColorProfile";
$tag_info{$exposureprogram}{desc}		= "ExposureProgram";
$tag_info{$spectralsensitivity}{desc}		= "SpectralSensitivity";
$tag_info{$gpsinfo}{desc}			= "GPSInfo";
$tag_info{$isospeedratings}{desc}		= "ISOSpeedRatings";
$tag_info{$oecf}{desc}				= "OECF";
$tag_info{$exifversion}{desc}			= "ExifVersion";
$tag_info{$datetimeoriginal}{desc}		= "DateTimeOriginal";
$tag_info{$datetimedigitized}{desc}		= "DateTimeDigitized";
$tag_info{$componentsconfiguration}{desc}	= "ComponentsConfiguration";
$tag_info{$compressedbitsperpixel}{desc}	= "CompressedBitsPerPixel";
$tag_info{$shutterspeedvalue}{desc}		= "ShutterSpeedValue";
$tag_info{$aperturevalue}{desc}			= "ApertureValue";
$tag_info{$brightnessvalue}{desc}		= "BrightnessValue";
$tag_info{$exposurebiasvalue}{desc}		= "ExposureBiasValue";
$tag_info{$maxaperturevalue}{desc}		= "MaxApertureValue";
$tag_info{$subjectdistance}{desc}		= "SubjectDistance";
$tag_info{$meteringmode}{desc}			= "MeteringMode";
$tag_info{$lightsource}{desc}			= "LightSource";
$tag_info{$flash}{desc}				= "Flash";
$tag_info{$focallength}{desc}			= "FocalLength";
$tag_info{$makernote}{desc}			= "MakerNote";
$tag_info{$usercomment}{desc}			= "UserComment";
$tag_info{$subsectime}{desc}			= "SubSecTime";
$tag_info{$subsectimeoriginal}{desc}		= "SubSecTimeOriginal";
$tag_info{$subsectimedigitized}{desc}		= "SubSecTimeDigitized";
$tag_info{$flashpixversion}{desc}		= "FlashPixVersion";
$tag_info{$colorspace}{desc}			= "ColorSpace";
$tag_info{$exifimagewidth}{desc}		= "ExifImageWidth";
$tag_info{$exifimagelength}{desc}		= "ExifImageLength";
$tag_info{$interoperabilityoffset}{desc}	= "InteroperabilityOffset";
$tag_info{$flashenergy}{desc}			= "FlashEnergy",
$tag_info{$spatialfrequencyresponse}{desc}	= "SpatialFrequencyResponse",
$tag_info{$focalplanexresolution}{desc}		= "FocalPlaneXResolution",
$tag_info{$focalplaneyresolution}{desc}		= "FocalPlaneYResolution",
$tag_info{$focalplaneresolutionunit}{desc}	= "FocalPlaneResolutionUnit",
$tag_info{$subjectlocation}{desc}		= "SubjectLocation",
$tag_info{$exposureindex}{desc}			= "ExposureIndex",
$tag_info{$sensingmethod}{desc}			= "SensingMethod",
$tag_info{$filesource}{desc}			= "FileSource";
$tag_info{$scenetype}{desc}			= "SceneType";

sub init_tags()
{
$tag_info{$newsubfiletype}{count} = 0;
$tag_info{$imagewidth}{count} = 0;
$tag_info{$imagelength}{count} = 0;
$tag_info{$bitspersample}{count} = 0;
$tag_info{$compression}{count} = 0;
$tag_info{$photometricinterpretation}{count} = 0;
$tag_info{$fillorder}{count} = 0;
$tag_info{$documentname}{count} = 0;
$tag_info{$imagedescription}{count} = 0;
$tag_info{$make}{count} = 0;
$tag_info{$model}{count} = 0;
$tag_info{$stripoffsets}{count} = 0;
$tag_info{$orientation}{count} = 0;
$tag_info{$samplesperpixel}{count} = 0;
$tag_info{$rowsperstrip}{count} = 0;
$tag_info{$stripbytecounts}{count} = 0;
$tag_info{$xresolution}{count} = 0;
$tag_info{$yresolution}{count} = 0;
$tag_info{$planarconfiguration}{count} = 0;
$tag_info{$resolutionunit}{count} = 0;
$tag_info{$transferfunction}{count} = 0;
$tag_info{$software}{count} = 0;
$tag_info{$datetime}{count} = 0;
$tag_info{$artist}{count} = 0;
$tag_info{$whitepoint}{count} = 0;
$tag_info{$primarychromaticities}{count} = 0;
$tag_info{$transferrange}{count} = 0;
$tag_info{$jpegproc}{count} = 0;
$tag_info{$jpeginterchangeformat}{count} = 0;
$tag_info{$jpeginterchangeformatlength}{count} = 0;
$tag_info{$ycbcrcoefficients}{count} = 0;
$tag_info{$ycbcrsubsampling}{count} = 0;
$tag_info{$ycbcrpositioning}{count} = 0;
$tag_info{$referenceblackwhite}{count} = 0;
$tag_info{$cfarepeatpatterndim}{count} = 0;
$tag_info{$cfapattern}{count} = 0;
$tag_info{$batterylevel}{count} = 0;
$tag_info{$copyright}{count} = 0;
$tag_info{$exposuretime}{count} = 0;
$tag_info{$fnumber}{count} = 0;
$tag_info{$iptc_naa}{count} = 0;
$tag_info{$exifoffset}{count} = 0;
$tag_info{$intercolorprofile}{count} = 0;
$tag_info{$exposureprogram}{count} = 0;
$tag_info{$spectralsensitivity}{count} = 0;
$tag_info{$gpsinfo}{count} = 0;
$tag_info{$isospeedratings}{count} = 0;
$tag_info{$oecf}{count} = 0;
$tag_info{$exifversion}{count} = 0;
$tag_info{$datetimeoriginal}{count} = 0;
$tag_info{$datetimedigitized}{count} = 0;
$tag_info{$componentsconfiguration}{count} = 0;
$tag_info{$compressedbitsperpixel}{count} = 0;
$tag_info{$shutterspeedvalue}{count} = 0;
$tag_info{$aperturevalue}{count} = 0;
$tag_info{$brightnessvalue}{count} = 0;
$tag_info{$exposurebiasvalue}{count} = 0;
$tag_info{$maxaperturevalue}{count} = 0;
$tag_info{$subjectdistance}{count} = 0;
$tag_info{$meteringmode}{count} = 0;
$tag_info{$lightsource}{count} = 0;
$tag_info{$flash}{count} = 0;
$tag_info{$focallength}{count} = 0;
$tag_info{$makernote}{count} = 0;
$tag_info{$usercomment}{count} = 0;
$tag_info{$subsectime}{count} = 0;
$tag_info{$subsectimeoriginal}{count} = 0;
$tag_info{$subsectimedigitized}{count} = 0;
$tag_info{$flashpixversion}{count} = 0;
$tag_info{$colorspace}{count} = 0;
$tag_info{$exifimagewidth}{count} = 0;
$tag_info{$exifimagelength}{count} = 0;
$tag_info{$interoperabilityoffset}{count} = 0;
$tag_info{$flashenergy}{count} = 0;
$tag_info{$spatialfrequencyresponse}{count} = 0;
$tag_info{$focalplanexresolution}{count} = 0;
$tag_info{$focalplaneyresolution}{count} = 0;
$tag_info{$focalplaneresolutionunit}{count} = 0;
$tag_info{$subjectlocation}{count} = 0;
$tag_info{$exposureindex}{count} = 0;
$tag_info{$sensingmethod}{count} = 0;
$tag_info{$filesource}{count} = 0;
$tag_info{$scenetype}{count} = 0;
}

sub handle_tag_specific($$)
{
  my $tiff = shift;
  my $tag = shift;

  if ($tag == $exifoffset)
    {
      my $value = $tag_info{$tag}{value};
#      printf "scanning next dir at offset 0x%08x\n", $value;
      scan_dir($tiff, $value);
    }
}

sub log_tag($$$$$$)
{
  my $tiff = shift;
  my $tag = shift;
  my $type = shift;
  my $count = shift;
  my $offset = shift;
  my $boffset = shift;

  if (!$tag_info{$tag}{desc})
    {
      printf "Tag 0x%04x: Unknown.\n", $tag;
      return;
    }

  $tag_info{$tag}{count}++;

  my $content = "";
  my $value = "";

#  if ($tag_info{$tag}{count} > 1)
#    {
#      printf "Note: tag 0x%04x redefined.\n", $tag;
#    }

  if ($type == 1 || $type == 7)
    {
      # Unsigned, unspecified byte
      $content = substr($boffset, 0, 1);
      $value = unpack('C', $boffset);
    }
  elsif ($type == 6)
    {
      # Signed byte
      $content = substr($boffset, 0, 1);
      $value = unpack('c', $boffset);
    }
  elsif ($type == 2)
    {
      # ASCII
      if ($count <= 4)
	{
	  $content = substr($boffset, 0, $count - 1);
	}
      else
	{
	  $content = substr($tiff, $offset, $count - 1);
	}

      $value = $content;
    }
  elsif ($type == 3 || $type == 8)
    {
      # Short
      if ($count <= 2)
	{
	  $content = substr($boffset, 0, 2 * $count);
	}
      else
	{
	  $content = substr($tiff, $offset, 2 * $count);
	}

      $value = unpack($short, $content);
    }
  elsif ($type == 4 || $type == 9)
    {
      # Long
      if ($count <= 1)
	{
	  $content = substr($boffset, 0, 4 * $count);
	}
      else
	{
	  $content = substr($tiff, $offset, 4 * $count);
	}

      $value = unpack($long, $content);
    }
  elsif($type == 5 || $type == 10)
    {
      # Rational
      $content = substr($tiff, $offset, 8 * $count);

      my $numerator;
      my $denominator;

      ($numerator, $denominator) = unpack($rational, $content);
      $value = $numerator / $denominator;
    }
  elsif ($type == 11)
    {
      #float
      $value = unpack("f", $boffset);
    }
  elsif ($type == 12)
    {
      $value = unpack("d", $boffset);
    }
  else
    {
      printf "Unhandled tag type $type for tag 0x%04x.\n", $tag;
    }

#  printf("Tag 0x%04x: type = %d, %s = $value\n", $tag, $type,
#	 $tag_info{$tag}{desc});

  $tag_info{$tag}{type} = $type;
  $tag_info{$tag}{count} = $count;
  $tag_info{$tag}{content} = $content;
  $tag_info{$tag}{value} = $value;

  handle_tag_specific($tiff, $tag);

#  printf("logging tag = %04x, desc = <%s>, count = %d.\n\n", 
#	 $tag, $tag_info{$tag}{desc}, $tag_info{$tag}{count});
}

sub scan_dir($$)
{
  my $tiff = shift;
  my $offset = shift;

  my $count = unpack($short, substr($tiff, $offset, 4));

#  printf "Scanning dir at offset 0x%08lx with $count entries.\n", $offset;

  $offset += 2;

  my $ii;
  my $t_tag;
  my $t_type;
  my $t_count;
  my $t_offset;

  for ($ii = 0; $ii < $count; $ii++)
    {
      ($t_tag, $t_type, $t_count, $t_offset) = 
	unpack($dirent, substr($tiff, $offset));

#      printf("Tag %04lx, type = $t_type, count = $t_count, offset = %08lx\n",
#	     $t_tag, $t_offset);

      log_tag($tiff, $t_tag, $t_type, $t_count, $t_offset,
	      substr($tiff, $offset + 8, 4));

      $offset += 12;
    }

  my $rval = unpack($long, substr($tiff, $offset));

#  printf "returning offset = 0x%08lx\n", $rval;
  return $rval;
}

sub dump_exif($)
{
  my $contents = shift;

  my $exif = substr($contents, 0, 4);

  if ($exif ne "Exif")
    {
     print "exdump: Not an EXIF file, missing \"Exif\" id\n";
     return;
    };

  my $tiff = substr($contents, 6);
  
  my $tiffid = substr($tiff, 0, 2);

  if ($tiffid eq "MM")
    {
      $short = "n";
      $long = "N";
    }
  elsif ($tiffid eq "II")
    {
      $short = "v";
      $long = "V";
    }
  else
    {
      die "exdump: Invalid TIFF identifier \"$tiffid\".\n";
    }

  $dirent = $short . $short . $long . $long;
  $rational = $long . $long;

  my $tiffarb = unpack($short, substr($tiff, 2, 2));

  $tiffarb == 42 || die "exdump: Invalid TIFF arbitrary \"$tiffarb\".\n";

  my $offset = unpack($long, substr($tiff, 4, 4));

  while ($offset = scan_dir($tiff, $offset))
    {
#      printf "returning offset = 0x%08lx\n", $offset;
    }

}

sub dump_jpeg($)
{
  my $file = shift;

  init_tags();

#  printf "scanning $file...\n";

  my $contents = "";

  open(FIN, $file) || die "exdump: Unable to open file $_";
  while (<FIN>)
    {
      $contents .= $_;
    }
  close FIN;

  dump_exif(substr($contents, 6));
  return %tag_info;
}

END {}

#if (@ARGV)
#  {
#    foreach (@ARGV)
#      {
##	eval			# Wrap the dump_exif call in an eval,
                                # so that if dump_exif dies, control
                                # returns here and we can try the next
                                # file.
#	  {
#	    dump_jpeg($_);
#	  }
#      }
#  }
#else
#  {
#    die "Usage: exdump file.jpg [ files.jpg ... ]\n";
#  }
