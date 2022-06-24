/*
 * example.c
 *
 * This file was part of the Independent JPEG Group's software.
 * Copyright (C) 1992-1996, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2017, 2019, 2022, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file illustrates how to use the IJG code as a subroutine library
 * to read or write JPEG image files with 8-bit or 12-bit data precision.  You
 * should look at this code in conjunction with the documentation file
 * libjpeg.txt.
 *
 * We present these routines in the same coding style used in the JPEG code
 * (ANSI function definitions, etc); but you are of course free to code your
 * routines in a different style if you prefer.
 */

/* First-time users of libjpeg-turbo might be better served by looking at
 * tjexample.c, which uses the more straightforward TurboJPEG API.  Note that
 * this example, like cjpeg and djpeg, interleaves disk I/O with JPEG
 * compression/decompression, so it is not suitable for benchmarking purposes.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define strcasecmp  stricmp
#define strncasecmp  strnicmp
#endif

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#include "jpeglib.h"
#include "jerror.h"
#include "jpeg12lib.h"
#include "j12error.h"

/*
 * <setjmp.h> is used for the optional error recovery mechanism shown in
 * the second part of the example.
 */

#include <setjmp.h>



/******************** JPEG COMPRESSION SAMPLE INTERFACE *******************/

/* This half of the example shows how to feed data into the JPEG compressor.
 * We present a minimal version that does not worry about refinements such
 * as error recovery (the JPEG code will just exit() if it gets an error).
 */


/*
 * IMAGE DATA FORMATS:
 *
 * The standard input image format is a rectangular array of pixels, with
 * each pixel having the same number of "component" values (color channels).
 * Each pixel row is an array of JSAMPLEs (which typically are unsigned chars).
 * If you are working with color data, then the color values for each pixel
 * must be adjacent in the row; for example, R,G,B,R,G,B,R,G,B,... for 24-bit
 * RGB color.
 *
 * For this example, we'll assume that this data structure matches the way
 * our application has stored the image in memory, so we can just pass a
 * pointer to our image buffer.  In particular, let's say that the image is
 * RGB color and is described by:
 */

#define WIDTH  640              /* Number of columns in image */
#define HEIGHT  480             /* Number of rows in image */


/*
 * Sample routine for JPEG compression with 8-bit data precision.  We assume
 * that the target file name and a compression quality factor are passed in.
 */

METHODDEF(void)
write_JPEG_file(char *filename, int quality)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE *outfile;                /* target file */
  JSAMPARRAY image_buffer;      /* Points to large array of R,G,B-order data */
  JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
  int row_stride;               /* physical row width in image buffer */
  int row, col;

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  if ((outfile = fopen(filename, "wb")) == NULL)
    ERREXIT(&cinfo, JERR_FILE_WRITE);
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = WIDTH;            /* image width and height, in pixels */
  cinfo.image_height = HEIGHT;
  cinfo.input_components = 3;           /* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB;       /* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
  /* Use 4:4:4 subsampling (default is 4:2:0) */
  cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 1;

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: allocate and initialize image buffer */

  row_stride = WIDTH * 3;       /* JSAMPLEs per row in image_buffer */
  /* Make a sample array that will go away when done with image.  Note that,
   * for the purposes of this example, we could also create a one-row-high
   * sample array and initialize it for each successive scanline written in the
   * scanline loop below.
   */
  image_buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, HEIGHT);

  /* Initialize image buffer with a repeating pattern */
  for (row = 0; row < HEIGHT; row++) {
    for (col = 0; col < WIDTH; col++) {
      image_buffer[row][col * 3] =
        (col * (MAXJSAMPLE + 1) / WIDTH) % (MAXJSAMPLE + 1);
      image_buffer[row][col * 3 + 1] =
        (row * (MAXJSAMPLE + 1) / HEIGHT) % (MAXJSAMPLE + 1);
      image_buffer[row][col * 3 + 2] =
        (row * (MAXJSAMPLE + 1) / HEIGHT + col * (MAXJSAMPLE + 1) / WIDTH) %
        (MAXJSAMPLE + 1);
    }
  }

  /* Step 6: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = image_buffer[cinfo.next_scanline];
    (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 7: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 8: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
}


/*
 * SOME FINE POINTS:
 *
 * In the above loop, we ignored the return value of jpeg_write_scanlines,
 * which is the number of scanlines actually written.  We could get away
 * with this because we were only relying on the value of cinfo.next_scanline,
 * which will be incremented correctly.  If you maintain additional loop
 * variables then you should be careful to increment them properly.
 * Actually, for output to a stdio stream you needn't worry, because
 * then jpeg_write_scanlines will write all the lines passed (or else exit
 * with a fatal error).  Partial writes can only occur if you use a data
 * destination module that can demand suspension of the compressor.
 * (If you don't know what that's for, you don't need it.)
 *
 * If the compressor requires full-image buffers (for entropy-coding
 * optimization or a multi-scan JPEG file), it will create temporary
 * files for anything that doesn't fit within the maximum-memory setting.
 * (Note that temp files are NOT needed if you use the default parameters.)
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.txt.
 *
 * Scanlines MUST be supplied in top-to-bottom order if you want your JPEG
 * files to be compatible with everyone else's.  If you cannot readily read
 * your data in that order, you'll need an intermediate array to hold the
 * image.  See rdtarga.c or rdbmp.c for examples of handling bottom-to-top
 * source data using the JPEG code's internal virtual-array mechanisms.
 */


/*
 * Sample routine for JPEG compression with 12-bit data precision.  We assume
 * that the target file name and a compression quality factor are passed in.
 */

#ifdef WITH_12BIT

METHODDEF(void)
write_JPEG12_file(char *filename, int quality)
{
  struct jpeg12_compress_struct cinfo;
  struct jpeg12_error_mgr jerr;
  FILE *outfile;
  J12SAMPARRAY image_buffer;
  J12SAMPROW row_pointer[1];
  int row_stride;
  int row, col;

  cinfo.err = jpeg12_std_error(&jerr);
  jpeg12_create_compress(&cinfo);

  if ((outfile = fopen(filename, "wb")) == NULL)
    J12ERREXIT(&cinfo, JERR_FILE_WRITE);
  jpeg12_stdio_dest(&cinfo, outfile);

  cinfo.image_width = WIDTH;
  cinfo.image_height = HEIGHT;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg12_set_defaults(&cinfo);
  jpeg12_set_quality(&cinfo, quality, TRUE);
  cinfo.comp_info[0].h_samp_factor = cinfo.comp_info[0].v_samp_factor = 1;

  jpeg12_start_compress(&cinfo, TRUE);

  row_stride = WIDTH * 3;
  image_buffer = (*cinfo.mem->alloc_sarray)
    ((j12_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, HEIGHT);

  /* Initialize image buffer with a repeating pattern */
  for (row = 0; row < HEIGHT; row++) {
    for (col = 0; col < WIDTH; col++) {
      image_buffer[row][col * 3] =
        (col * (MAXJ12SAMPLE + 1) / WIDTH) % (MAXJ12SAMPLE + 1);
      image_buffer[row][col * 3 + 1] =
        (row * (MAXJ12SAMPLE + 1) / HEIGHT) % (MAXJ12SAMPLE + 1);
      image_buffer[row][col * 3 + 2] =
        (row * (MAXJ12SAMPLE + 1) / HEIGHT +
         col * (MAXJ12SAMPLE + 1) / WIDTH) % (MAXJ12SAMPLE + 1);
    }
  }

  row_stride = WIDTH * 3;

  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = image_buffer[cinfo.next_scanline];
    (void)jpeg12_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg12_finish_compress(&cinfo);
  fclose(outfile);

  jpeg12_destroy_compress(&cinfo);
}

#endif



/******************** JPEG DECOMPRESSION SAMPLE INTERFACE *******************/

/* This half of the example shows how to read data from the JPEG decompressor.
 * It's a bit more refined than the above, in that we show:
 *   (a) how to modify the JPEG library's standard error-reporting behavior;
 *   (b) how to allocate workspace using the library's memory manager.
 *
 * Just to make this example a little different from the first one, we'll
 * assume that we do not intend to put the whole image into an in-memory
 * buffer, but to send it line-by-line someplace else.  We need a one-
 * scanline-high JSAMPLE array as a work buffer, and we will let the JPEG
 * memory manager allocate it for us.  This approach is actually quite useful
 * because we don't need to remember to deallocate the buffer separately: it
 * will go away automatically when the JPEG object is cleaned up.
 */


/*
 * ERROR HANDLING:
 *
 * The JPEG library's standard error handler (jerror.c) is divided into
 * several "methods" which you can override individually.  This lets you
 * adjust the behavior without duplicating a lot of code, which you might
 * have to update with each future release.
 *
 * Our example here shows how to override the "error_exit" method so that
 * control is returned to the library's caller when a fatal error occurs,
 * rather than calling exit() as the standard error_exit method does.
 *
 * We use C's setjmp/longjmp facility to return control.  This means that the
 * routine which calls the JPEG library must first execute a setjmp() call to
 * establish the return point.  We want the replacement error_exit to do a
 * longjmp().  But we need to make the setjmp buffer accessible to the
 * error_exit routine.  To do this, we make a private extension of the
 * standard JPEG error handler object.  (If we were using C++, we'd say we
 * were making a subclass of the regular error handler.)
 *
 * Here's the extended error handler struct:
 */

struct my_error_mgr {
  struct jpeg_error_mgr pub;    /* "public" fields */

  jmp_buf setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr)cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


METHODDEF(int) do_read_JPEG_file(struct jpeg_decompress_struct *cinfo,
                                 char *infilename, char *outfilename);

/*
 * Sample routine for JPEG decompression with 8-bit data precision.  We assume
 * that the source file name is passed in.  We want to return 1 on success, 0
 * on error.
 */

METHODDEF(int)
read_JPEG_file(char *infilename, char *outfilename)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;

  return do_read_JPEG_file(&cinfo, infilename, outfilename);
}

/*
 * We call the libjpeg API from within a separate function, because modifying
 * the local non-volatile jpeg_decompress_struct instance below the setjmp()
 * return point and then accessing the instance after setjmp() returns would
 * result in undefined behavior that may potentially overwrite all or part of
 * the structure.
 */

METHODDEF(int)
do_read_JPEG_file(struct jpeg_decompress_struct *cinfo, char *infilename,
                  char *outfilename)
{
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  FILE *infile;                 /* source file */
  FILE *outfile;                /* output file */
  JSAMPARRAY buffer;            /* Output row buffer */
  int row_stride;               /* physical row width in output buffer */

  /* In this example we want to open the input and output files before doing
   * anything else, so that the setjmp() error recovery below can assume the
   * files are open.
   *
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read/write binary files.
   */

  if ((infile = fopen(infilename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", infilename);
    return 0;
  }
  if ((outfile = fopen(outfilename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", outfilename);
    fclose(infile);
    return 0;
  }
  /* emit header for raw PPM format */
  fprintf(outfile, "P6\n%d %d\n%d\n", WIDTH, HEIGHT, MAXJSAMPLE);

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo->err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(cinfo);
    fclose(infile);
    fclose(outfile);
    return 0;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(cinfo);

  /* Step 2: specify data source (eg, a file) */

  jpeg_stdio_src(cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void)jpeg_read_header(cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.txt for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void)jpeg_start_decompress(cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo->output_width * cinfo->output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo->output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo->output_scanline < cinfo->output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void)jpeg_read_scanlines(cinfo, buffer, 1);
    fwrite(buffer[0], 1, row_stride, outfile);
  }

  /* Step 7: Finish decompression */

  (void)jpeg_finish_decompress(cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(cinfo);

  /* After finish_decompress, we can close the input and output files.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
  fclose(infile);
  fclose(outfile);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return 1;
}


/*
 * SOME FINE POINTS:
 *
 * In the above code, we ignored the return value of jpeg_read_scanlines,
 * which is the number of scanlines actually read.  We could get away with
 * this because we asked for only one line at a time and we weren't using
 * a suspending data source.  See libjpeg.txt for more info.
 *
 * We cheated a bit by calling alloc_sarray() after jpeg_start_decompress();
 * we should have done it beforehand to ensure that the space would be
 * counted against the JPEG max_memory setting.  In some systems the above
 * code would risk an out-of-memory error.  However, in general we don't
 * know the output image dimensions before jpeg_start_decompress(), unless we
 * call jpeg_calc_output_dimensions().  See libjpeg.txt for more about this.
 *
 * Scanlines are returned in the same order as they appear in the JPEG file,
 * which is standardly top-to-bottom.  If you must emit data bottom-to-top,
 * you can use one of the virtual arrays provided by the JPEG memory manager
 * to invert the data.  See wrbmp.c for an example.
 *
 * As with compression, some operating modes may require temporary files.
 * On some systems you may need to set up a signal handler to ensure that
 * temporary files are deleted if the program is interrupted.  See libjpeg.txt.
 */


#ifdef WITH_12BIT

struct my12_error_mgr {
  struct jpeg12_error_mgr pub;

  jmp_buf setjmp_buffer;
};

typedef struct my12_error_mgr *my12_error_ptr;

METHODDEF(void)
my12_error_exit(j12_common_ptr cinfo)
{
  my12_error_ptr myerr = (my12_error_ptr)cinfo->err;

  (*cinfo->err->output_message) (cinfo);

  longjmp(myerr->setjmp_buffer, 1);
}

METHODDEF(int) do_read_JPEG12_file(struct jpeg12_decompress_struct *cinfo,
                                   char *infilename, char *outfilename);

/*
 * Sample routine for JPEG decompression with 12-bit data precision.  We assume
 * that the source file name is passed in.  We want to return 1 on success, 0
 * on error.
 */

METHODDEF(int)
read_JPEG12_file(char *infilename, char *outfilename)
{
  struct jpeg12_decompress_struct cinfo;

  return do_read_JPEG12_file(&cinfo, infilename, outfilename);
}

METHODDEF(int)
do_read_JPEG12_file(struct jpeg12_decompress_struct *cinfo, char *infilename,
                    char *outfilename)
{
  struct my12_error_mgr jerr;
  FILE *infile;
  FILE *outfile;
  J12SAMPARRAY buffer;
  int row_stride;
  int col;

  if ((infile = fopen(infilename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", infilename);
    return 0;
  }
  if ((outfile = fopen(outfilename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", outfilename);
    fclose(infile);
    return 0;
  }
  fprintf(outfile, "P6\n%d %d\n%d\n", WIDTH, HEIGHT, MAXJ12SAMPLE);

  cinfo->err = jpeg12_std_error(&jerr.pub);
  jerr.pub.error_exit = my12_error_exit;
  if (setjmp(jerr.setjmp_buffer)) {
    jpeg12_destroy_decompress(cinfo);
    fclose(infile);
    fclose(outfile);
    return 0;
  }
  jpeg12_create_decompress(cinfo);

  jpeg12_stdio_src(cinfo, infile);

  (void)jpeg12_read_header(cinfo, TRUE);

  (void)jpeg12_start_decompress(cinfo);

  row_stride = cinfo->output_width * cinfo->output_components;
  buffer = (*cinfo->mem->alloc_sarray)
    ((j12_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

  while (cinfo->output_scanline < cinfo->output_height) {
    (void)jpeg12_read_scanlines(cinfo, buffer, 1);
    /* Swap MSB and LSB in each sample */
    for (col = 0; col < row_stride; col++)
      buffer[0][col] = ((buffer[0][col] & 0xFF) << 8) |
                       ((buffer[0][col] >> 8) & 0xFF);
    fwrite(buffer[0], 1, row_stride * sizeof(J12SAMPLE), outfile);
  }

  (void)jpeg12_finish_decompress(cinfo);

  jpeg12_destroy_decompress(cinfo);

  fclose(infile);
  fclose(outfile);

  return 1;
}

#endif



LOCAL(void)
usage(const char *progname)
{
  fprintf(stderr, "usage: %s compress [switches] outputfile[.jpg]\n",
          progname);
  fprintf(stderr, "       %s decompress inputfile[.jpg] outputfile[.ppm]\n",
          progname);
  fprintf(stderr, "Switches (names may be abbreviated):\n");
#ifdef WITH_12BIT
  fprintf(stderr, "  -12bit         Compress/decompress JPEG file with 12-bit data precision\n");
#endif
  fprintf(stderr, "  -quality N     Compression quality (0..100; 5-95 is most useful range,\n");
  fprintf(stderr, "                 default is 75)\n");

  exit(EXIT_FAILURE);
}


typedef enum {
  COMPRESS,
  DECOMPRESS
} EXAMPLE_MODE;


int
main(int argc, char **argv)
{
  int argn, quality = 75;
#ifdef WITH_12BIT
  int _12bit = 0;
#endif
  EXAMPLE_MODE mode = -1;
  char *arg, *filename = NULL;

  if (argc < 3)
    usage(argv[0]);

  if (!strcasecmp(argv[1], "compress"))
    mode = COMPRESS;
  else if (!strcasecmp(argv[1], "decompress"))
    mode = DECOMPRESS;
  else
    usage(argv[0]);

  for (argn = 2; argn < argc; argn++) {
    arg = argv[argn];
    if (*arg != '-') {
      filename = arg;
      /* Not a switch, must be a file name argument */
      break;                    /* done parsing switches */
    }
    arg++;                      /* advance past switch marker character */

#ifdef WITH_12BIT
    if (!strncasecmp(arg, "1", 1))
      _12bit = 1;
    else
#endif
    if (!strncasecmp(arg, "q", 1)) {
      /* Quality rating (quantization table scaling factor). */
      if (++argn >= argc)       /* advance to next argument */
        usage(argv[0]);
      if (sscanf(argv[argn], "%d", &quality) < 1 || quality < 0 ||
          quality > 100)
        usage(argv[0]);
      if (quality < 1)
        quality = 1;
    }
  }

  if (!filename)
    usage(argv[0]);

  if (mode == COMPRESS) {
#ifdef WITH_12BIT
    if (_12bit)
      write_JPEG12_file(filename, quality);
    else
#endif
      write_JPEG_file(filename, quality);
  } else if (mode == DECOMPRESS) {
    if (argc - argn < 2)
      usage(argv[0]);

#ifdef WITH_12BIT
    if (_12bit)
      read_JPEG12_file(argv[argn], argv[argn + 1]);
    else
#endif
      read_JPEG_file(argv[argn], argv[argn + 1]);
  }

  return 0;
}
