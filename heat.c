/*********************************************************************************/
/*                                                                               */
/*  Animation of heat equation in a planar domain                                */
/*                                                                               */
/*  N. Berglund, May 2021                                                        */
/*                                                                               */
/*  Feel free to reuse, but if doing so it would be nice to drop a               */
/*  line to nils.berglund@univ-orleans.fr - Thanks!                              */
/*                                                                               */
/*  compile with                                                                 */
/*  gcc -o heat heat.c                                                           */
/* -L/usr/X11R6/lib -ltiff -lm -lGL -lGLU -lX11 -lXmu -lglut -O3 -fopenmp        */
/*                                                                               */
/*  To make a video, set MOVIE to 1 and create subfolder tif_heat                */
/*  It may be possible to increase parameter PAUSE                               */
/*                                                                               */
/*  create movie using                                                           */
/*  ffmpeg -i wave.%05d.tif -vcodec libx264 wave.mp4                             */
/*                                                                               */
/*********************************************************************************/

/*********************************************************************************/
/*                                                                               */
/* NB: The algorithm used to simulate the wave equation is highly paralellizable */
/* One could make it much faster by using a GPU                                  */
/*                                                                               */
/*********************************************************************************/

#include <math.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/glu.h>
#include <unistd.h>
#include <sys/types.h>
#include <tiffio.h>     /* Sam Leffler's libtiff library. */
#include <omp.h>

#define MOVIE 0         /* set to 1 to generate movie */

/* General geometrical parameters */

#define WINWIDTH 	1280  /* window width */
#define WINHEIGHT 	720   /* window height */

#define NX 1280          /* number of grid points on x axis */
#define NY 720          /* number of grid points on y axis */
// #define NX 640          /* number of grid points on x axis */
// #define NY 360          /* number of grid points on y axis */

/* setting NX to WINWIDTH and NY to WINHEIGHT increases resolution */
/* but will multiply run time by 4                                 */

#define XMIN -2.0
#define XMAX 2.0	/* x interval */
// #define XMIN -1.5
// #define XMAX 2.5	/* x interval */
#define YMIN -1.125
#define YMAX 1.125	/* y interval for 9/16 aspect ratio */

#define JULIA_SCALE 1.1 /* scaling for Julia sets */
// #define JULIA_SCALE 0.8 /* scaling for Julia sets */

/* Choice of the billiard table */

#define B_DOMAIN 25      /* choice of domain shape */

#define D_RECTANGLE 0   /* rectangular domain */
#define D_ELLIPSE 1     /* elliptical domain */
#define D_STADIUM 2     /* stadium-shaped domain */
#define D_SINAI 3       /* Sinai billiard */
#define D_DIAMOND 4     /* diamond-shaped billiard */
#define D_TRIANGLE 5    /* triangular billiard */
#define D_FLAT 6        /* flat interface */
#define D_ANNULUS 7     /* annulus */
#define D_POLYGON 8     /* polygon */
#define D_YOUNG 9       /* Young diffraction slits */
#define D_GRATING 10    /* diffraction grating */
#define D_EHRENFEST 11  /* Ehrenfest urn type geometry */

#define D_MENGER 15     /* Menger-Sierpinski carpet */ 
#define D_JULIA_INT 16  /* interior of Julia set */ 

/* Billiard tables for heat equation */

#define D_ANNULUS_HEATED 21 /* annulus with different temperatures */
#define D_MENGER_HEATED 22  /* Menger gasket with different temperatures */
#define D_MENGER_H_OPEN 23  /* Menger gasket with different temperatures and larger domain */
#define D_MANDELBROT 24     /* Mandelbrot set */
#define D_JULIA 25          /* Julia set */
#define D_MANDELBROT_CIRCLE 26     /* Mandelbrot set with circular conductor */

#define LAMBDA 0.7	    /* parameter controlling the dimensions of domain */
#define MU 0.1	            /* parameter controlling the dimensions of domain */
#define NPOLY 6             /* number of sides of polygon */
#define APOLY 1.0           /* angle by which to turn polygon, in units of Pi/2 */
#define MDEPTH 2            /* depth of computation of Menger gasket */
#define MRATIO 5            /* ratio defining Menger gasket */
#define MANDELLEVEL 1000      /* iteration level for Mandelbrot set */
#define MANDELLIMIT 10.0     /* limit value for approximation of Mandelbrot set */
#define FOCI 1              /* set to 1 to draw focal points of ellipse */

/* You can add more billiard tables by adapting the functions */
/* xy_in_billiard and draw_billiard in sub_wave.c */

/* Physical patameters of wave equation */

// #define DT 0.00001
#define DT 0.000004
// #define DT 0.000002
// #define DT 0.00000002
// #define DT 0.000000005
#define VISCOSITY 10.0
#define T_OUT 2.0       /* outside temperature */
#define T_IN 0.0        /* inside temperature */
// #define T_OUT 0.0       /* outside temperature */
// #define T_IN 2.0        /* inside temperature */
#define SPEED 0.0       /* speed of drift to the right */

/* Boundary conditions */

#define B_COND 0

#define BC_DIRICHLET 0   /* Dirichlet boundary conditions */
#define BC_PERIODIC 1    /* periodic boundary conditions */
#define BC_ABSORBING 2   /* absorbing boundary conditions (beta version) */

/* Parameters for length and speed of simulation */

#define NSTEPS 4500      /* number of frames of movie */
#define NVID 50          /* number of iterations between images displayed on screen */
// #define NVID 100          /* number of iterations between images displayed on screen */
#define NSEG 100         /* number of segments of boundary */

#define PAUSE 100       /* number of frames after which to pause */
#define PSLEEP 1         /* sleep time during pause */
#define SLEEP1  2        /* initial sleeping time */
#define SLEEP2  1        /* final sleeping time */

/* For debugging purposes only */
#define FLOOR 0         /* set to 1 to limit wave amplitude to VMAX */
#define VMAX 10.0       /* max value of wave amplitude */

/* Field representation */

#define FIELD_REP 0

#define F_INTENSITY 0   /* color represents intensity */
#define F_GRADIENT 1    /* color represents norm of gradient */ 

#define DRAW_FIELD_LINES 1  /* set to 1 to draw field lines */
#define FIELD_LINE_WIDTH 1  /* width of field lines */
#define N_FIELD_LINES 200   /* number of field lines */
#define FIELD_LINE_FACTOR 100 /* factor controlling precision when computing origin of field lines */

/* Color schemes */

#define BLACK 1          /* black background */

#define COLOR_SCHEME 1   /* choice of color scheme */

#define C_LUM 0          /* color scheme modifies luminosity (with slow drift of hue) */
#define C_HUE 1          /* color scheme modifies hue */
#define C_PHASE 2        /* color scheme shows phase */

#define SCALE 0          /* set to 1 to adjust color scheme to variance of field */
// #define SLOPE 0.1        /* sensitivity of color on wave amplitude */
#define SLOPE 0.3        /* sensitivity of color on wave amplitude */
#define ATTENUATION 0.0  /* exponential attenuation coefficient of contrast with time */

#define COLORHUE 260     /* initial hue of water color for scheme C_LUM */
#define COLORDRIFT 0.0   /* how much the color hue drifts during the whole simulation */
#define LUMMEAN 0.5      /* amplitude of luminosity variation for scheme C_LUM */
#define LUMAMP 0.3       /* amplitude of luminosity variation for scheme C_LUM */
#define HUEMEAN 280.0    /* mean value of hue for color scheme C_HUE */
#define HUEAMP -110.0      /* amplitude of variation of hue for color scheme C_HUE */
// #define HUEMEAN 270.0    /* mean value of hue for color scheme C_HUE */
// #define HUEAMP -130.0      /* amplitude of variation of hue for color scheme C_HUE */

/* Basic math */

#define PI 	3.141592654
#define DPI 	6.283185307
#define PID 	1.570796327

double julia_x = 0.0, julia_y = 0.0;    /* parameters for Julia sets */


#include "sub_wave.c"

double courant2;  /* Courant parameter squared */
double dx2;       /* spatial step size squared */
double intstep;   /* integration step */
double intstep1;  /* integration step used in absorbing boundary conditions */



void init_gaussian(x, y, mean, amplitude, scalex, phi, xy_in)
/* initialise field with gaussian at position (x,y) */
    double x, y, mean, amplitude, scalex, *phi[NX]; 
    short int * xy_in[NX];

{
    int i, j, in;
    double xy[2], dist2, module, phase, scale2;    

    scale2 = scalex*scalex;
    printf("Initialising field\n");
    for (i=0; i<NX; i++)
        for (j=0; j<NY; j++)
        {
            ij_to_xy(i, j, xy);
	    xy_in[i][j] = xy_in_billiard(xy[0],xy[1]);

            in = xy_in[i][j];
            if (in == 1)
            {
                dist2 = (xy[0]-x)*(xy[0]-x) + (xy[1]-y)*(xy[1]-y);
                module = amplitude*exp(-dist2/scale2);
                if (module < 1.0e-15) module = 1.0e-15;

                phi[i][j] = mean + module/scalex;
            }   /* boundary temperatures */
            else if (in >= 2) phi[i][j] = T_IN*pow(0.75, (double)(in-2));
//             else if (in >= 2) phi[i][j] = T_IN*pow(1.0 - 0.5*(double)(in-2), (double)(in-2));
//             else if (in >= 2) phi[i][j] = T_IN*(1.0 - (double)(in-2)/((double)MDEPTH))*(1.0 - (double)(in-2)/((double)MDEPTH));
            else phi[i][j] = T_OUT;
        }
}

void init_julia_set(phi, xy_in)
/* change Julia set boundary condition */
    double *phi[NX]; 
    short int * xy_in[NX];

{
    int i, j, in;
    double xy[2], dist2, module, phase, scale2;    

//     printf("Changing Julia set\n");
    for (i=0; i<NX; i++)
        for (j=0; j<NY; j++)
        {
            ij_to_xy(i, j, xy);
	    xy_in[i][j] = xy_in_billiard(xy[0],xy[1]);

            in = xy_in[i][j];
            if (in >= 2) phi[i][j] = T_IN;
        }
}


/*********************/
/* animation part    */
/*********************/


void compute_gradient(phi, nablax, nablay)
/* compute the gradient of the field */
double *phi[NX], *nablax[NX], *nablay[NX];
{
    int i, j, iplus, iminus, jplus, jminus; 
    double dx;
    
    dx = (XMAX-XMIN)/((double)NX);
    
    for (i=0; i<NX; i++)
        for (j=0; j<NY; j++)
        {
            iplus = i+1;  if (iplus == NX) iplus = NX-1;
            iminus = i-1; if (iminus == -1) iminus = 0;
            jplus = j+1;  if (jplus == NX) jplus = NY-1;
            jminus = j-1; if (jminus == -1) jminus = 0;
            nablax[i][j] = (phi[iplus][j] - phi[iminus][j])/dx;
            nablay[i][j] = (phi[i][jplus] - phi[i][jminus])/dx;
        }
}

void draw_field_line(x, y, xy_in, nablax, nablay, delta, nsteps)
// void draw_field_line(x, y, nablax, nablay, delta, nsteps)
/* draw a field line of the gradient, starting in (x,y) */
double x, y, *nablax[NX], *nablay[NX], delta;
int nsteps;
short int *xy_in[NX];
{
    double x1, y1, x2, y2, pos[2], nabx, naby, norm2, norm;
    int i = 0, ij[2], cont = 1;
    
    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(FIELD_LINE_WIDTH);
    x1 = x;
    y1 = y;
    
//     printf("Drawing field line \n");

    glEnable(GL_LINE_SMOOTH);
    glBegin(GL_LINE_STRIP);
    xy_to_pos(x1, y1, pos);
    glVertex2d(pos[0], pos[1]);
    
    i = 0;
    while ((cont)&&(i < nsteps))
    {
        xy_to_ij(x1, y1, ij);
        
        if (ij[0] < 0) ij[0] = 0;
        if (ij[0] > NX-1) ij[0] = NX-1;
        if (ij[1] < 0) ij[1] = 0;
        if (ij[1] > NY-1) ij[1] = NY-1;
        
        nabx = nablax[ij[0]][ij[1]];
        naby = nablay[ij[0]][ij[1]];
        
        norm2 = nabx*nabx + naby*naby;
        
        if (norm2 > 1.0e-14)
        {
            /* avoid too large step size */
            if (norm2 < 1.0e-9) norm2 = 1.0e-9;
            norm = sqrt(norm2);
            x1 = x1 + delta*nabx/norm;
            y1 = y1 + delta*naby/norm;
        }
        else 
        {
            cont = 0;
//             nablax[ij[0]][ij[1]] = 0.0;
//             nablay[ij[0]][ij[1]] = 0.0;            
        }
        
        if (!xy_in[ij[0]][ij[1]]) cont = 0;
        
        /* stop if the boundary is hit */
//         if (xy_in[ij[0]][ij[1]] != 1) cont = 0;
        
//         printf("x1 = %.3lg \t y1 = %.3lg \n", x1, y1);
                
        xy_to_pos(x1, y1, pos);
        glVertex2d(pos[0], pos[1]);
        
        i++;
    }
    glEnd();
}

void draw_wave(phi, xy_in, scale, time)
/* draw the field */
double *phi[NX], scale;
short int *xy_in[NX];
int time;
{
    int i, j, iplus, iminus, jplus, jminus, ij[2], counter = 0;
    static int first = 1;
    double rgb[3], xy[2], x1, y1, x2, y2, dx, value, angle, dangle, intens, deltaintens, sum = 0.0;
    double *nablax[NX], *nablay[NX];
    static double linex[N_FIELD_LINES*FIELD_LINE_FACTOR], liney[N_FIELD_LINES*FIELD_LINE_FACTOR], distance[N_FIELD_LINES*FIELD_LINE_FACTOR], integral[N_FIELD_LINES*FIELD_LINE_FACTOR + 1];

    for (i=0; i<NX; i++) 
    {
        nablax[i] = (double *)malloc(NY*sizeof(double));
        nablay[i] = (double *)malloc(NY*sizeof(double));
    }
    
    /* compute the gradient */
//     if (FIELD_REP > 0) 
    compute_gradient(phi, nablax, nablay);
    
    /* compute the position of origins of field lines */
    if ((first)&&(DRAW_FIELD_LINES))
    {
        first = 0;
        
        printf("computing linex\n");
        
        x1 = sqrt(3.58);
        y1 = 0.0;
        linex[0] = x1;
        liney[0] = y1;
        dangle = DPI/((double)(N_FIELD_LINES*FIELD_LINE_FACTOR));
            
        for (i = 1; i < N_FIELD_LINES*FIELD_LINE_FACTOR; i++)
        {
//             angle = PID + (double)i*dangle;
            angle = (double)i*dangle;
            x2 = sqrt(3.58)*cos(angle);
            y2 = sqrt(1.18)*sin(angle);
            linex[i] = x2;
            liney[i] = y2;
            distance[i-1] = module2(x2-x1,y2-y1);
            x1 = x2;
            y1 = y2;
        }
        distance[N_FIELD_LINES*FIELD_LINE_FACTOR - 1] = module2(x2-sqrt(3.58),y2);
    }

    dx = (XMAX-XMIN)/((double)NX);
    glBegin(GL_QUADS);

    for (i=0; i<NX; i++)
        for (j=0; j<NY; j++)
        {
            if (FIELD_REP == F_INTENSITY) value = phi[i][j];
            else if (FIELD_REP == F_GRADIENT)
            {
                value = module2(nablax[i][j], nablay[i][j]);
            }
            
//             if ((phi[i][j] - T_IN)*(phi[i][j] - T_OUT) < 0.0) 
            if (xy_in[i][j] == 1) 
            {
                color_scheme(COLOR_SCHEME, value, scale, time, rgb);
                glColor3f(rgb[0], rgb[1], rgb[2]);
            }
            else glColor3f(0.0, 0.0, 0.0);

            glVertex2i(i, j);
            glVertex2i(i+1, j);
            glVertex2i(i+1, j+1);
            glVertex2i(i, j+1);
        }
    glEnd ();
        
    /* draw a field line */
    if (DRAW_FIELD_LINES)
    {
        /* compute gradient norm along boundary and its integral */
        for (i = 0; i < N_FIELD_LINES*FIELD_LINE_FACTOR; i++)
        {
            xy_to_ij(linex[i], liney[i], ij);
            intens = module2(nablax[ij[0]][ij[1]], nablay[ij[0]][ij[1]])*distance[i];
            if (i > 0) integral[i] = integral[i-1] + intens;
            else integral[i] = intens;
        }
        deltaintens = integral[N_FIELD_LINES*FIELD_LINE_FACTOR-1]/((double)N_FIELD_LINES);
//         deltaintens = integral[N_FIELD_LINES*FIELD_LINE_FACTOR-1]/((double)N_FIELD_LINES + 1.0);
//         deltaintens = integral[N_FIELD_LINES*FIELD_LINE_FACTOR-1]/((double)N_FIELD_LINES);
        
//         printf("delta = %.5lg\n", deltaintens);
        
        i = 0;
//         draw_field_line(linex[0], liney[0], nablax, nablay, 0.00002, 100000);
        draw_field_line(linex[0], liney[0], xy_in, nablax, nablay, 0.00002, 100000);
        for (j = 1; j < N_FIELD_LINES+1; j++)
        {
            while ((integral[i] <= j*deltaintens)&&(i < N_FIELD_LINES*FIELD_LINE_FACTOR)) i++; 
//             draw_field_line(linex[i], liney[i], nablax, nablay, 0.00002, 100000);
            draw_field_line(linex[i], liney[i], xy_in, nablax, nablay, 0.00002, 100000);
            counter++;
        }
        printf("%i lines\n", counter);
    }

    
    for (i=0; i<NX; i++)
    {
        free(nablax[i]);
        free(nablay[i]);
    }
}



void evolve_wave(phi, xy_in)
/* time step of field evolution */
    double *phi[NX]; short int *xy_in[NX];
{
    int i, j, iplus, iminus, jplus, jminus;
    double delta1, delta2, x, y, *newphi[NX];;
    
    for (i=0; i<NX; i++) newphi[i] = (double *)malloc(NY*sizeof(double));

    #pragma omp parallel for private(i,j,iplus,iminus,jplus,jminus,delta1,delta2,x,y)
    for (i=0; i<NX; i++){
        for (j=0; j<NY; j++){
            if (xy_in[i][j] == 1){
                /* discretized Laplacian depending on boundary conditions */
                if ((B_COND == BC_DIRICHLET)||(B_COND == BC_ABSORBING))
                {
                    iplus = (i+1);   if (iplus == NX) iplus = NX-1;
                    iminus = (i-1);  if (iminus == -1) iminus = 0;
                    jplus = (j+1);   if (jplus == NY) jplus = NY-1;
                    jminus = (j-1);  if (jminus == -1) jminus = 0;
                }
                else if (B_COND == BC_PERIODIC)
                {
                    iplus = (i+1) % NX;
                    iminus = (i-1) % NX;
                    if (iminus < 0) iminus += NX;
                    jplus = (j+1) % NY;
                    jminus = (j-1) % NY;
                    if (jminus < 0) jminus += NY;
                }
                
                delta1 = phi[iplus][j] + phi[iminus][j] + phi[i][jplus] + phi[i][jminus] - 4.0*phi[i][j];

                x = phi[i][j];

                /* evolve phi */
                if (B_COND != BC_ABSORBING)
                {
                    newphi[i][j] = x + intstep*(delta1 - SPEED*(phi[iplus][j] - phi[i][j]));
                }
                else        /* case of absorbing b.c. - this is only an approximation of correct way of implementing */
                {
                    /* in the bulk */
                    if ((i>0)&&(i<NX-1)&&(j>0)&&(j<NY-1))
                    {
                        newphi[i][j] = x - intstep*delta2;
                    }
                     /* right border */
                    else if (i==NX-1) 
                    {
                        newphi[i][j] = x - intstep1*(x - phi[i-1][j]);
                    }
                    /* upper border */
                    else if (j==NY-1) 
                    {
                        newphi[i][j] = x - intstep1*(x - phi[i][j-1]);
                    }
                    /* left border */
                    else if (i==0) 
                    {
                        newphi[i][j] = x - intstep1*(x - phi[1][j]);
                    }
                   /* lower border */
                    else if (j==0) 
                    {
                        newphi[i][j] = x - intstep1*(x - phi[i][1]);
                    }
                }


                if (FLOOR)
                {
                    if (newphi[i][j] > VMAX) phi[i][j] = VMAX;
                    if (newphi[i][j] < -VMAX) phi[i][j] = -VMAX;
                }
            }
        }
    }
    
    for (i=0; i<NX; i++){
        for (j=0; j<NY; j++){
            if (xy_in[i][j] == 1) phi[i][j] = newphi[i][j];
        }
    }
    
    for (i=0; i<NX; i++)
    {
        free(newphi[i]);
    }

//     printf("phi(0,0) = %.3lg, psi(0,0) = %.3lg\n", phi[NX/2][NY/2], psi[NX/2][NY/2]);
}




double compute_variance(phi, xy_in)
/* compute the variance (total probability) of the field */
double *phi[NX]; short int * xy_in[NX];
{
    int i, j, n = 0;
    double variance = 0.0;

    for (i=1; i<NX; i++)
        for (j=1; j<NY; j++)
        {
            if (xy_in[i][j])
            {
                n++;
                variance += phi[i][j]*phi[i][j];
            }
        }
    if (n==0) n=1;
    return(variance/(double)n);
}

void renormalise_field(phi, xy_in, variance)
/* renormalise variance of field */
double *phi[NX], variance; 
short int * xy_in[NX];
{
    int i, j;
    double stdv;
    
    stdv = sqrt(variance);

    for (i=1; i<NX; i++)
        for (j=1; j<NY; j++)
        {
            if (xy_in[i][j])
            {
                phi[i][j] = phi[i][j]/stdv;
            }
        }
}

void print_level(level)
int level;
{
    double pos[2];
    char message[50];
    
    glColor3f(1.0, 1.0, 1.0);
    sprintf(message, "Level %i", level);
    xy_to_pos(XMIN + 0.1, YMAX - 0.2, pos);
    write_text(pos[0], pos[1], message);
}


void print_Julia_parameters()
{
    double pos[2];
    char message[50];
    
    glColor3f(1.0, 1.0, 1.0);
    if (julia_y >= 0.0) sprintf(message, "c = %.5f + %.5f i", julia_x, julia_y);
    else sprintf(message, "c = %.5f %.5f i", julia_x, julia_y);
    xy_to_pos(XMIN + 0.1, YMAX - 0.2, pos);
    write_text(pos[0], pos[1], message);
}

void set_Julia_parameters(time, phi, xy_in)
int time;
double *phi[NX];
short int *xy_in[NX];
{
    double jangle, cosj, sinj, radius = 0.15;

    jangle = (double)time*DPI/(double)NSTEPS;
//     jangle = (double)time*0.001;
//     jangle = (double)time*0.0001;

    cosj = cos(jangle);
    sinj = sin(jangle);
    julia_x = -0.9 + radius*cosj;
    julia_y = radius*sinj;
    init_julia_set(phi, xy_in);
    
    printf("Julia set parameters : i = %i, angle = %.5lg, cx = %.5lg, cy = %.5lg \n", time, jangle, julia_x, julia_y);
}

void set_Julia_parameters_cardioid(time, phi, xy_in)
int time;
double *phi[NX];
short int *xy_in[NX];
{
    double jangle, cosj, sinj, yshift;

    jangle = pow(1.05 + (double)time*0.00003, 0.333);
    yshift = 0.02*sin((double)time*PID*0.002);
//     jangle = pow(1.0 + (double)time*0.00003, 0.333);
//     jangle = pow(0.05 + (double)time*0.00003, 0.333);
//     jangle = pow(0.1 + (double)time*0.00001, 0.333);
//     yshift = 0.04*sin((double)time*PID*0.002);

    cosj = cos(jangle);
    sinj = sin(jangle);
    julia_x = 0.5*(cosj*(1.0 - 0.5*cosj) + 0.5*sinj*sinj);
    julia_y = 0.5*sinj*(1.0-cosj) + yshift;
    /* need to decrease 0.05 for i > 2000 */
//     julia_x = 0.5*(cosj*(1.0 - 0.5*cosj) + 0.5*sinj*sinj);
//     julia_y = 0.5*sinj*(1.0-cosj);
    init_julia_set(phi, xy_in);
    
    printf("Julia set parameters : i = %i, angle = %.5lg, cx = %.5lg, cy = %.5lg \n", time, jangle, julia_x, julia_y);
}

void animation()
{
    double time, scale, dx, var, jangle, cosj, sinj;
    double *phi[NX];
    short int *xy_in[NX];
    int i, j, s;

    /* Since NX and NY are big, it seemed wiser to use some memory allocation here */
    for (i=0; i<NX; i++)
    {
        phi[i] = (double *)malloc(NY*sizeof(double));
        xy_in[i] = (short int *)malloc(NY*sizeof(short int));
    }

    dx = (XMAX-XMIN)/((double)NX);
    intstep = DT/(dx*dx*VISCOSITY);
    intstep1 = DT/(dx*VISCOSITY);
    
//     julia_x = 0.1; 
//     julia_y = 0.6; 
    
    set_Julia_parameters(0, phi, xy_in);
    
    printf("Integration step %.3lg\n", intstep);

    /* initialize wave wave function */
    init_gaussian(-1.0, 0.0, 0.1, 0.0, 0.01, phi, xy_in);
//     init_gaussian(x, y, mean, amplitude, scalex, phi, xy_in)
    
    if (SCALE)
    {
        var = compute_variance(phi, xy_in);
        scale = sqrt(1.0 + var);
        renormalise_field(phi, xy_in, var);
    }

    blank();
    glColor3f(0.0, 0.0, 0.0);
    

    glutSwapBuffers();
    
    draw_wave(phi, xy_in, 1.0, 0);
    draw_billiard();
    print_Julia_parameters(i);
    
//     print_level(MDEPTH);

    glutSwapBuffers();

    sleep(SLEEP1);
    if (MOVIE) for (i=0; i<SLEEP1*25; i++) save_frame();

    for (i=0; i<=NSTEPS; i++)
    {
        /* compute the variance of the field to adjust color scheme */
        /* the color depends on the field divided by sqrt(1 + variance) */
        if (SCALE)
        {
            var = compute_variance(phi, xy_in);
            scale = sqrt(1.0 + var);
//             printf("Norm: %5lg\t Scaling factor: %5lg\n", var, scale);
            renormalise_field(phi, xy_in, var);
        }
        else scale = 1.0;
        
        draw_wave(phi, xy_in, scale, i);
        
        for (j=0; j<NVID; j++) evolve_wave(phi, xy_in);

        draw_billiard();
        
//         print_level(MDEPTH);
        print_Julia_parameters(i);

	glutSwapBuffers();
        
        /* modify Julia set */
        set_Julia_parameters(i, phi, xy_in);

	if (MOVIE)
        {
            save_frame();

            /* it seems that saving too many files too fast can cause trouble with the file system */
            /* so this is to make a pause from time to time - parameter PAUSE may need adjusting   */
            if (i % PAUSE == PAUSE - 1)
            {
                printf("Making a short pause\n");
                sleep(PSLEEP);
                s = system("mv wave*.tif tif_heat/");
            }
        }

    }

    if (MOVIE)
    {
        for (i=0; i<20; i++) save_frame();
        s = system("mv wave*.tif tif_heat/");
    }
    for (i=0; i<NX; i++)
    {
        free(phi[i]);
    }

}


void display(void)
{
    glPushMatrix();

    blank();
    glutSwapBuffers();
    blank();
    glutSwapBuffers();

    animation();
    sleep(SLEEP2);

    glPopMatrix();

    glutDestroyWindow(glutGetWindow());

}


int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(WINWIDTH,WINHEIGHT);
    glutCreateWindow("Heat equation in a planar domain");

    init();

    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}

