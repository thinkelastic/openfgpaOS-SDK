/* gears.c — 3D gear wheels for openfpgaOS Pocket
 *
 * Adapted from the TinyGL raw example by Brian Paul.
 * Renders via TinyGL software rasterizer into the Pocket's RGB565 framebuffer.
 *
 * Resolution : 320x240
 * Color mode : RGB565 (OF_VIDEO_MODE_RGB565)
 * TinyGL mode: ZB_MODE_5R6G5B (16-bit, matches RGB565 directly)
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <of.h>
#include <TGL/gl.h>
#include "zbuffer.h"

/* M_PI is a POSIX extension not defined by the SDK libc — define it here */
#ifndef M_PI
#define M_PI 3.1415926f
#endif

/* ── FPS counter ─────────────────────────────────────────────────── */
static uint32_t fps_last_ms    = 0;
static int      fps_frame_count = 0;
static int      fps_display     = 0;
static char     fps_str[12];

/* ── Screen dimensions ──────────────────────────────────────────── */
#define WIN_W  OF_SCREEN_W   /* 320 */
#define WIN_H  OF_SCREEN_H   /* 240 */

/* ── Gear state ─────────────────────────────────────────────────── */
static GLfloat view_rotx = 20.0f, view_roty = 30.0f;
static GLint   gear1, gear2, gear3;
static GLfloat angle = 0.0f;

static uint16_t zbuf[WIN_W*WIN_H*2] __attribute__((aligned(4)));

/* ── Stipple pattern ────────────────────────────────────────────── */
static GLubyte stipplepattern[128] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA,
    0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55,
    0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,

    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA,
    0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55,
    0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,

    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA,
    0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55,
    0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,

    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA,
    0xAA, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55,
    0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
};

int override_drawmodes;

/* 
 * Draw a Star Fox style spaceship using triangles
 */
static void drawStarFoxShip()
{
    static GLfloat yellow[4] = {1.0, 1.0, 0.0, 0.0};
    static GLfloat orange[4] = {1.0, 0.5, 0.0, 0.0};
    static GLfloat darkred[4] = {0.8, 0.0, 0.0, 0.0};
    
    glMaterialfv(GL_FRONT, GL_DIFFUSE, yellow);
    glMaterialfv(GL_FRONT, GL_SPECULAR, orange);
    glColor3fv(yellow);
    
    // Main fuselage (central body) - series of triangles
    glBegin(GL_TRIANGLES);
    
    // Front nose cone (tip)
    glNormal3f(0.0, 0.0, 1.0);
    glVertex3f(0.0, 0.0, 2.0);    // nose tip
    glVertex3f(0.3, 0.0, 1.2);    // right side
    glVertex3f(-0.3, 0.0, 1.2);   // left side
    
    // Right side of fuselage
    glNormal3f(0.3, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 2.0);
    glVertex3f(0.3, 0.0, 1.2);
    glVertex3f(0.2, 0.0, -1.5);
    
    // Left side of fuselage
    glNormal3f(-0.3, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 2.0);
    glVertex3f(-0.2, 0.0, -1.5);
    glVertex3f(-0.3, 0.0, 1.2);
    
    glEnd();
    
    // Left wing
    glBegin(GL_TRIANGLES);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, orange);
    glColor3fv(orange);
    
    // Main left wing surface
    glNormal3f(-0.1, 0.3, 0.0);
    glVertex3f(0.0, 0.0, 0.5);     // root
    glVertex3f(-0.3, 0.0, 1.2);    // front root
    glVertex3f(-1.5, 1.2, 0.2);    // wing tip
    
    glNormal3f(-0.1, 0.3, 0.0);
    glVertex3f(-0.3, 0.0, 1.2);
    glVertex3f(-0.2, 0.0, -1.5);
    glVertex3f(-1.5, 1.2, 0.2);
    
    glEnd();
    
    // Right wing (mirror of left)
    glBegin(GL_TRIANGLES);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, orange);
    glColor3fv(orange);
    
    // Main right wing surface
    glNormal3f(0.1, 0.3, 0.0);
    glVertex3f(0.0, 0.0, 0.5);
    glVertex3f(1.5, 1.2, 0.2);
    glVertex3f(0.3, 0.0, 1.2);
    
    glNormal3f(0.1, 0.3, 0.0);
    glVertex3f(0.3, 0.0, 1.2);
    glVertex3f(1.5, 1.2, 0.2);
    glVertex3f(0.2, 0.0, -1.5);
    
    glEnd();
    
    // Tail section
    glBegin(GL_TRIANGLES);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, darkred);
    glColor3fv(darkred);
    
    // Vertical tail fin
    glNormal3f(0.0, 0.0, 1.0);
    glVertex3f(0.0, 0.0, -1.5);    // tail root
    glVertex3f(0.0, 0.8, -2.0);    // tail tip
    glVertex3f(0.15, 0.1, -1.8);   // right side
    
    glNormal3f(0.0, 0.0, 1.0);
    glVertex3f(0.0, 0.0, -1.5);
    glVertex3f(-0.15, 0.1, -1.8);  // left side
    glVertex3f(0.0, 0.8, -2.0);
    
    // Horizontal stabilizers (left)
    glNormal3f(-0.2, -0.2, -1.0);
    glVertex3f(0.0, 0.0, -1.5);
    glVertex3f(-0.5, -0.3, -2.0);
    glVertex3f(-0.2, 0.1, -1.8);
    
    // Horizontal stabilizers (right)
    glNormal3f(0.2, -0.2, -1.0);
    glVertex3f(0.0, 0.0, -1.5);
    glVertex3f(0.2, 0.1, -1.8);
    glVertex3f(0.5, -0.3, -2.0);
    
    glEnd();
    
    // Landing gear/struts (3 legs)
    glBegin(GL_TRIANGLES);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, yellow);
    glColor3fv(yellow);
    
    // Front landing gear
    glNormal3f(0.0, -1.0, 0.0);
    glVertex3f(0.0, 0.0, 0.8);
    glVertex3f(0.1, -0.8, 0.7);
    glVertex3f(-0.1, -0.8, 0.7);
    
    // Left landing gear
    glNormal3f(-1.0, -0.5, 0.0);
    glVertex3f(-0.3, 0.0, -0.2);
    glVertex3f(-0.5, -0.8, -0.1);
    glVertex3f(-0.2, -0.8, 0.1);
    
    // Right landing gear
    glNormal3f(1.0, -0.5, 0.0);
    glVertex3f(0.3, 0.0, -0.2);
    glVertex3f(0.2, -0.8, 0.1);
    glVertex3f(0.5, -0.8, -0.1);
    
    glEnd();
}

//Modificar la función draw() para incluir la nave:
// void draw()
// {
//     angle += 2.0;
//     glPushMatrix();
//     glRotatef(view_rotx, 1.0, 0.0, 0.0);
//     glRotatef(view_roty, 0.0, 1.0, 0.0);

//     // Render the Star Fox spaceship
//     glPushMatrix();
//     glTranslatef(0.0, 2.0, -3.0);  // Posiciona la nave en el centro
//     glRotatef(angle * 0.5, 0.0, 1.0, 0.0);  // Gira suavemente
//     glScalef(1.5, 1.5, 1.5);  // Escala para hacerla más grande
//     drawStarFoxShip();
//     glPopMatrix();

//     glPopMatrix();
// }

/*
 * Draw a gear wheel.  You'll probably want to call this function when
 * building a display list since we do a lot of trig here.
 *
 * Input:  inner_radius - radius of hole at center
 *         outer_radius - radius at center of teeth
 *         width - width of gear
 *         teeth - number of teeth
 *         tooth_depth - depth of tooth
 */
static void gear(GLfloat inner_radius,
                 GLfloat outer_radius,
                 GLfloat width,
                 GLint teeth,
                 GLfloat tooth_depth,
                 int override_drawmodes)
{
    GLfloat r0, r1, r2;
    GLfloat angle, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0f;
    r2 = outer_radius + tooth_depth / 2.0f;
    da = 2.0f * M_PI / teeth / 4.0f;

    glShadeModel(GL_FLAT);

    /* ── Cara frontal (normal fija apuntando +Z) ───────────────── */
    glNormal3f(0.0f, 0.0f, 1.0f);

    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUAD_STRIP);

    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;
        glVertex3f(r0 * cos(angle), r0 * sin(angle),  width * 0.5f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle),  width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * cos(angle), r0 * sin(angle),  width * 0.5f);
            glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da), width * 0.5f);
        }
    }
    glEnd();

    /* ── Dientes cara frontal ──────────────────────────────────── */
    glNormal3f(0.0f, 0.0f, 1.0f);

    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUADS);

    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;
        glVertex3f(r1 * cos(angle),        r1 * sin(angle),        width * 0.5f);
        glVertex3f(r2 * cos(angle + da),   r2 * sin(angle + da),   width * 0.5f);
        glVertex3f(r2 * cos(angle + 2*da), r2 * sin(angle + 2*da), width * 0.5f);
        glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da), width * 0.5f);
    }
    glEnd();

    /* ── Cara trasera (normal fija apuntando -Z) ───────────────── */
    glNormal3f(0.0f, 0.0f, -1.0f);

    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUAD_STRIP);

    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;
        glVertex3f(r1 * cos(angle),        r1 * sin(angle),        -width * 0.5f);
        glVertex3f(r0 * cos(angle),        r0 * sin(angle),        -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da), -width * 0.5f);
            glVertex3f(r0 * cos(angle),        r0 * sin(angle),        -width * 0.5f);
        }
    }
    glEnd();

    /* ── Dientes cara trasera ──────────────────────────────────── */
    glNormal3f(0.0f, 0.0f, -1.0f);

    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUADS);

    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;
        glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da), -width * 0.5f);
        glVertex3f(r2 * cos(angle + 2*da), r2 * sin(angle + 2*da), -width * 0.5f);
        glVertex3f(r2 * cos(angle + da),   r2 * sin(angle + da),   -width * 0.5f);
        glVertex3f(r1 * cos(angle),        r1 * sin(angle),        -width * 0.5f);
    }
    glEnd();

    /* ── Caras laterales de los dientes (outward faces) ────────── */
    /* Con GL_FLAT cada quad necesita su propia normal antes del bloque.
     * GL_QUAD_STRIP agrupa pares → emitimos la normal justo antes
     * de cada par de vértices del mismo plano. */
    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUAD_STRIP);

    for (GLint i = 0; i < teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;

        /* Flanco de bajada r1→r2: normal perpendicular al segmento */
        u = r2 * cos(angle + da) - r1 * cos(angle);
        v = r2 * sin(angle + da) - r1 * sin(angle);
        len = sqrt(u*u + v*v);
        glNormal3f( v/len, -u/len, 0.0f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle),  width * 0.5f);
        glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5f);

        /* Tope del diente primer segmento: normal radial */
        glNormal3f(cos(angle + da), sin(angle + da), 0.0f);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da),  width * 0.5f);
        glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5f);

        /* Tope del diente segundo segmento */
        glNormal3f(cos(angle + 2*da), sin(angle + 2*da), 0.0f);
        glVertex3f(r2 * cos(angle + 2*da), r2 * sin(angle + 2*da),  width * 0.5f);
        glVertex3f(r2 * cos(angle + 2*da), r2 * sin(angle + 2*da), -width * 0.5f);

        /* Flanco de subida r2→r1 */
        u = r1 * cos(angle + 3*da) - r2 * cos(angle + 2*da);
        v = r1 * sin(angle + 3*da) - r2 * sin(angle + 2*da);
        len = sqrt(u*u + v*v);
        glNormal3f( v/len, -u/len, 0.0f);
        glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da),  width * 0.5f);
        glVertex3f(r1 * cos(angle + 3*da), r1 * sin(angle + 3*da), -width * 0.5f);
    }

    /* Vértice de cierre del strip */
    glNormal3f(cos(0.0f), sin(0.0f), 0.0f);
    glVertex3f(r1 * cos(0.0f), r1 * sin(0.0f),  width * 0.5f);
    glVertex3f(r1 * cos(0.0f), r1 * sin(0.0f), -width * 0.5f);
    glEnd();

    /* ── Cilindro interior (normal apunta hacia el centro, -radial) */
    if (override_drawmodes == 1)       glBegin(GL_LINES);
    else if (override_drawmodes == 2)  glBegin(GL_POINTS);
    else                               glBegin(GL_QUAD_STRIP);

    for (GLint i = 0; i <= teeth; i++) {
        angle = i * 2.0f * M_PI / teeth;
        glNormal3f(-cos(angle), -sin(angle), 0.0f); /* ← descomentado y ANTES del par */
        glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5f);
        glVertex3f(r0 * cos(angle), r0 * sin(angle),  width * 0.5f);
    }
    glEnd();
}

static void draw()
{
    angle += 2.0;
    glPushMatrix();
    glRotatef(view_rotx, 1.0, 0.0, 0.0);
    glRotatef(view_roty, 0.0, 1.0, 0.0);

    glPushMatrix();
    glTranslatef(-3.0, -2.0, 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1, -2.0, 0.0);
    glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1, 4.2, 0.0);
    glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();
}

void initScene()
{
    // static GLfloat pos[4] = {0.408248290463863, 0.408248290463863,
    // 0.816496580927726, 0.0 }; //Light at infinity.
    static GLfloat pos[4] = {5, 5, 10, 0.0};  // Light at infinity.
    // static GLfloat pos[4] = {5, 5, -10, 0.0}; // Light at infinity.

    static GLfloat red[4] = {1.0, 0.0, 0.0, 0.0};
    static GLfloat green[4] = {0.0, 1.0, 0.0, 0.0};
    static GLfloat blue[4] = {0.0, 0.0, 1.0, 0.0};
    static GLfloat white[4] = {1.0, 1.0, 1.0, 0.0};
    static GLfloat shininess = 5;
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
    // glLightfv( GL_LIGHT0, GL_AMBIENT, white);
    glLightfv(GL_LIGHT0, GL_SPECULAR, white);
    glEnable(GL_CULL_FACE);

    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    //glEnable(GL_POLYGON_STIPPLE);
    //	glDisable(GL_POLYGON_STIPPLE);
    glPolygonStipple(stipplepattern);
    glPointSize(2.0f);
    glTextSize(GL_TEXT_SIZE24x24);
    /* make the gears */
    gear1 = glGenLists(1);
    glNewList(gear1, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, red);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
    glColor3fv(red);
    gear(1.0, 4.0, 1.0, 20, 0.7, override_drawmodes);  // The largest gear.
    glEndList();

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, green);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glColor3fv(green);
    gear(0.5, 2.0, 2.0, 10,
         0.7, override_drawmodes);  // The small gear with the smaller hole, to the right.
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, blue);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white);
    glColor3fv(blue);
    gear(1.3, 2.0, 0.5, 10, 0.7, override_drawmodes);  // The small gear above with the large hole.
    glEndList();
    glEnable( GL_NORMALIZE );
}

/* ── Entry point ────────────────────────────────────────────────── */
int main(void)
{
    int winSizeX = 320, winSizeY = 240;

    /* Switch Pocket framebuffer to RGB565 — matches TinyGL ZB_MODE_5R6G5B */
    of_video_init();
    //of_video_set_display_mode(OF_DISPLAY_OVERLAY);
    //printf("Initialise TinyGL...\n");

    of_video_set_color_mode(OF_VIDEO_MODE_RGB565);
    

    /* Initialise TinyGL with the same 320x240 dimensions */
    //PIXEL *imbuf = calloc(1, sizeof(PIXEL) * winSizeX * winSizeY);
    ZBuffer *zb = ZB_open(WIN_W, WIN_H, ZB_MODE_5R6G5B, zbuf);
    if (!zb) {
        //printf("\nZB_open failed!");
        return 1;   /* nothing sensible to do without a framebuffer */
    }
       

    glInit(zb);
    // initialize GL:

    /* GL setup */
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, WIN_W, WIN_H);
    glShadeModel(GL_FLAT);
    glEnable(GL_LIGHTING);
    //glSetEnableSpecular(GL_TRUE);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    GLfloat h = (GLfloat)WIN_H / (GLfloat)WIN_W;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0*h, 1.0*h, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -45.0f);

    initScene();

    fps_last_ms = of_time_ms();
    

    /* Main loop */
    while (1) {
        of_input_poll();

        /* Rotate with d-pad */
        if (of_btn_pressed(OF_BTN_UP))    view_rotx -= 2.0f;
        if (of_btn_pressed(OF_BTN_DOWN))  view_rotx += 2.0f;
        if (of_btn_pressed(OF_BTN_LEFT))  view_roty -= 2.0f;
        if (of_btn_pressed(OF_BTN_RIGHT)) view_roty += 2.0f;
        if (of_btn_pressed(OF_BTN_A)) {
            override_drawmodes++;
            if (override_drawmodes > 2) override_drawmodes=0;
            initScene();
        }


        /* Exit to menu */
        if (of_btn_pressed(OF_BTN_START))
            break;

        // draw scene:
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw();
        glDrawText((GLubyte *)fps_str, 4, 4, 0xFFFF00);
        // glDrawText((GLubyte *) "RED text", 0, 0, 0xFF0000);
        // glDrawText((GLubyte *) "GREEN text", 0, 24, 0x00FF00);
        // glDrawText((GLubyte *) "BLUE text", 0, 48, 0xFF);

        /* Copy TinyGL output directly to Pocket framebuffer.
         * Both are RGB565, so no pixel conversion needed.
         * Stride = WIN_W * 2 bytes (one uint16_t per pixel). */
        uint16_t *fb = of_video_surface16();
        ZB_copyFrameBuffer(zb, fb, WIN_W * sizeof(uint16_t));
        //of_video_flush();
        of_video_flip();
        //of_video_sync();

                /* ── FPS ─────────────────────────────────────────────────── */
        fps_frame_count++;
        uint32_t now_ms = of_time_ms();
        uint32_t elapsed = now_ms - fps_last_ms;   /* uint32 → wrap-safe */

        if (elapsed >= 1000) {
            fps_display     = (fps_frame_count * 1000 + elapsed / 2) / elapsed;
            fps_frame_count = 0;
            fps_last_ms     = now_ms;
        }

        /* Formato manual: evita sprintf/stdio */
        {
            char *p = fps_str;
            *p++ = 'F'; *p++ = 'P'; *p++ = 'S'; *p++ = ':'; *p++ = ' ';
            int v = fps_display;
            if      (v >= 100) { *p++ = '0' + v/100; v %= 100; *p++ = '0' + v/10; *p++ = '0' + v%10; }
            else if (v >=  10) {                               *p++ = '0' + v/10; *p++ = '0' + v%10; }
            else               {                                                   *p++ = '0' + v;     }
            *p = '\0';
        }
    }

    /* Cleanup */
    glDeleteList(gear1);
    glDeleteList(gear2);
    glDeleteList(gear3);
    //ZB_close(zb);
    glClose();

    /* Restore default 8-bit mode before returning to OS */
    of_video_set_color_mode(OF_VIDEO_MODE_8BIT);

    return 0;
}