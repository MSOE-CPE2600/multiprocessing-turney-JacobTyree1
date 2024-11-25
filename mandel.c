/// 
//  Jacob Tyree, Lab12, 121
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  To test time, simply input time in front of the other commands. 
//  
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "jpegrw.h"
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#define MAX_THREADS 20

typedef struct {
    imgRawImage *img;
    double xmin, xmax, ymin, ymax;
    int max, color_scheme, thread_id, num_threads;
} ThreadData;


// local routines
static int iteration_to_color( int i, int max, int color_scheme );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, 
									int color_scheme, int num_threads );
static void show_help();

void *thread_compute(void *arg);


int main(int argc, char *argv[]) {

    char c;

    // Default configuration values
    const char *outfile = "mandel";
    double xcenter = 0;
    double ycenter = 0;
    double xscale = 4;
    int image_width = 1000;
    int image_height = 1000;
    int num_processes = 1;
    int max = 1000;
    int total_images = 50;
    int num_threads = 1;

    // Parse command-line arguments
    while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:n:t:T:h")) != -1) {
        switch (c) {
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                xscale = atof(optarg);
                break;
            case 'W':
                image_width = atoi(optarg);
                break;
            case 'H':
                image_height = atoi(optarg);
                break;
            case 'm':
                max = atoi(optarg);
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'n':
                num_processes = atoi(optarg);
                break;
            case 't':
                total_images = atoi(optarg);
                break;
            case 'T':
                num_threads = atoi(optarg);
                if (num_threads < 1 || num_threads > MAX_THREADS) {
                    fprintf(stderr, "Error: Number of threads must be between 1 and 20\n");
                    exit(1);
                }
                break;
            case 'h':
                show_help();
                exit(1);
                break;
        }
    }

    printf("mandel: x=%lf y=%lf xscale=%lf max=%d outfile=%s\n",
           xcenter, ycenter, xscale, max, outfile);

    int images_per_process = total_images / num_processes;
    int remaining_images = total_images % num_processes;

    for (int p = 0; p < num_processes; p++) {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            int start_image = p * images_per_process;
            int end_image = start_image + images_per_process;

            // Assigning remaining images to the last process
            if (p == num_processes - 1) {
                end_image += remaining_images;
            }

            for (int i = start_image; i < end_image; i++) {
                double scale = xscale / (1.0 + i * 0.1);
                double yscale = scale / image_width * image_height;

                // Giving each file a unique name
                char filename[256];
                snprintf(filename, sizeof(filename), "%s_%d.jpg", outfile, i);

                // Create a raw image of the appropriate size
                imgRawImage *img = initRawImage(image_width, image_height);
                setImageCOLOR(img, 0); // Fill with a black

                // Compute the Mandelbrot image
                compute_image(img,
                              xcenter - scale / 2,
                              xcenter + scale / 2,
                              ycenter - yscale / 2,
                              ycenter + yscale / 2,
                              max, 
							  i,//Index is the color scheme
                              num_threads); 

                // Save the image to the unique filename
                storeJpegImageFile(img, filename);

                // Free the image resources
                freeRawImage(img);
            }
            exit(0); // Child process exits
        } else if (pid < 0) {
            perror("Fork failed"); //Error checking
            exit(1);
        }
    }

    // Wait for all child processes to finish
    for (int p = 0; p < num_processes; p++) {
        wait(NULL);
    }
    printf("All images generated.\n");

    return 0;
}





/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int color_scheme, int num_threads )
{

    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];

    for(int t = 0; t < num_threads; t++) {
        thread_data[t] = (ThreadData){
            .img = img,
            .xmin = xmin,
            .xmax = xmax, 
            .ymin = ymin,
            .ymax = ymax, 
            .max = max,
            .color_scheme = color_scheme, 
            .thread_id = t, 
            .num_threads = num_threads, 
        };
        pthread_create(&threads[t], NULL, thread_compute, &thread_data[t]);
    }
    //Waiting for threads to complete
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }
	/*int i,j;

	int width = img->width;
	int height = img->height;

	// For every pixel in the image...

	for(j=0;j<height;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max, color_scheme));
		}
	} */
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max, int color_scheme )
{
	double t = (double)iters / max;
	int r, g, b;

	switch (color_scheme % 4) {
		case 0:
			r = g = b = (int)(255 * t);
			break;
		case 1:
			r = (int)(255 * t);
			g = 0;
			b = (int)(255 * (1-t));
			break;
		case 2:
			r = (int)(128 + 127 * sin(6.28 * t + 0));
			g = (int)(128 + 127 * sin(6.28 * t + 2 * M_PI / 3));
			b = (int)(128 + 127 * sin(6.28 * t + 4 * M_PI / 3));
			break;
		case 3:
			r = (iters % 16) * 16;
			g = (iters % 32) * 8;
			b = (iters % 64) * 4;
			break;
	}
	return (r << 16) | (g << 8) | b;

	//int color = 0xFFFFFF*iters/(double)max;
	//return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

void *thread_compute(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    int width = data->img->width;
    int height = data->img->height;

    int start_row = (data->thread_id * height) / data->num_threads;
    int end_row = ((data->thread_id + 1) * height) / data->num_threads;

    for(int j = start_row; j < end_row; j++) {
        for (int i = 0; i < width; i ++) {
            double x = data->xmin + i * (data->xmax - data->xmin) / width;
            double y = data->ymin + j * (data->ymax - data->ymin) / height;
            int iters = iterations_at_point(x, y, data->max);
            setPixelCOLOR(data->img, i, j, iteration_to_color(iters, data->max, data->color_scheme));
        }
    }
    pthread_exit(NULL);
}