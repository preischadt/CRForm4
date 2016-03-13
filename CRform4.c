#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<time.h>
#include<omp.h>
#include<allegro.h>

#define HALF_X 400
#define HALF_Y 300
#define PI 3.14159265
#define INITIAL_BALLS 6000
#define EXPLOSION_RADIUS 30
#define BALL_RADIUS 4.5
#define EXPLOSION_TIME 35
#define EXPLOSION_FORCE (0.5*EXPLOSION_TIME/0x20)
#define INERTIA 0.99
#define TRAIL_SIZE 10
#define ZOOM 0.1
#define MUTATION_RATE 20
#define MUTATION_CHANCE 4000
#define MIN_DIVERSITY 1.2
#define SIMULATION_LENGTH 900
#define CAMERA_SPEED (15.0/camera_zoom)
#define CAMERA_ZOOM 3
#define CAMERA_ZOOM_SPEED 0.1
#define CHUNK_SIZE 80
#define CHUNK_DIST 5
#define MINI_PROP 0.25
#define MAX_EXPLOSIONS ((balls/EXPLOSION_TIME)*10000)
#define MAX_DISTANCE (CHUNK_SIZE*chunk_dist)
#define WAIT 8000
#define min(x,y) (x<y?x:y)
#define max(x,y) (x>y?x:y)
#define PERMEABILITY 4
#define BARRIER_DIST 15
#define BARRIER_START 500

typedef struct{
	float x,y;
	float vx,vy;
	int red, green, blue;
	int time;
	int hits;
	struct chunk *chunk;
	struct ballNode *node;
}Ball;

typedef struct ballNode{
	Ball *ball;
	struct ballNode *prev, *next;
}BallNode;

typedef struct ballList{
	BallNode *head;
}BallList;

typedef struct chunk{
	struct chunk *up, *down, *left, *right;
	struct{
		float x,y;
	}min;
	struct{
		float x,y;
	}max;
	BallList list;
}Chunk;

int my_random(int min, int max);
Chunk *get_pos_chunk(float x, float y);
void move_to_chunk(Chunk *chunk, Ball *ball);
void create_chunks(void);
void diversity(void);
int explode(Chunk *chunk, float x, float y, int red, int green, int blue, float chunk_modifier);
void count_down(void);
void show(void);
void show_mini(void);
void move(void);
void smart_rest(int duration);

BITMAP *buffer; 
Ball *b;
Chunk ***chunk;
int click_flag, click_x=0, click_y=0, balls, cx, cy, follow=-2, fastest, barrier_width, barrier_height, barriers;
float zoom, desvio, camera_zoom, chunk_width, chunk_height, camx, camy;
long long unsigned iteration=0;

void start(void){
	long long unsigned seed;
	printf("Seed (0 for clock): ");
	scanf("%llu", &seed);
	if(!seed) seed = time(NULL);
	srand(seed);
	printf("%llu\n", seed);
	allegro_init();
	install_keyboard();
	install_mouse();
	install_timer();
	set_color_depth(desktop_color_depth());
	set_gfx_mode(GFX_AUTODETECT_WINDOWED, HALF_X*2, HALF_Y*2, 0, 0);
	//set_gfx_mode(GFX_AUTODETECT, HALF_X*2, HALF_Y*2, 0, 0);
	install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL);
}

int main(void){
	int i, follow_flag=0, big_bang;
	
	printf("Balls (0 for default): ");
	scanf("%d", &balls);
	if(!balls) balls = INITIAL_BALLS;
	printf("%d\n", balls);
	b = malloc(balls*sizeof(Ball));
	
	printf("Big Bang Style (0 for no, 1 for yes): ");
	scanf("%d", &big_bang);
	
	printf("Barriers (0 for no, 1 for yes): ");
	scanf("%d", &barriers);
	
	start();
	
	
	printf("\nCreated by Lucas Preischadt Pinheiro\n\n"
			"Controls:\n"
			"\tArrowKeys: move camera\n"
			"\tPage Up: zoom in\n"
			"\tPage Down: zoom out\n"
			"\tSpacebar: reset zoom\n"
			"\tS: follow next\n"
			"\tX: follow previous\n"
			"\tW: cancel follow\n"
			"\tClick and Drag: analyse region\n"
			"\tEsc: quit\n");
	
	zoom = ZOOM;
	camera_zoom = CAMERA_ZOOM;
	camx = 0;
	camy = 0;
	for(i=0; i<balls; i++){
		if(big_bang){
			b[i].x = my_random(-100, 100)/100.0;
			b[i].y = my_random(-100, 100)/100.0;
		}else{
			b[i].x = my_random(-HALF_X/zoom, HALF_X/zoom);
			b[i].y = my_random(-HALF_Y/zoom, HALF_Y/zoom);
		}
		b[i].vx = 0.0;
		b[i].vy = 0.0;
		b[i].red = rand()%0x80;
		b[i].green = rand()%0x80;
		b[i].blue = rand()%0x80;
		b[i].time = my_random(1, EXPLOSION_TIME);
		//b[i].time = EXPLOSION_TIME*my_random(7,13)/10;
		b[i].hits = 0;
		b[i].chunk = NULL;
		b[i].node = malloc(sizeof(BallNode));
		b[i].node->ball = &b[i];
		b[i].node->prev = NULL;
		b[i].node->next = NULL;
	}
	create_chunks();
	
	buffer = create_bitmap(2*HALF_X, 2*HALF_Y);
	
	clear(buffer);
	show();
	show_mini();
	//Mostrar frame:
	blit(buffer, screen, 0, 0, 0, 0, HALF_X*2, HALF_Y*2);
	if(big_bang) smart_rest(WAIT);
	
	while(!key[KEY_ESC]){ //Loop Principal
		//Controles:
		if(key[KEY_LEFT] || key[KEY_4_PAD]){
			camx -= CAMERA_SPEED;
		}
		if(key[KEY_RIGHT] || key[KEY_6_PAD]){
			camx += CAMERA_SPEED;
		}
		if(key[KEY_UP] || key[KEY_8_PAD]){
			camy -= CAMERA_SPEED;
		}
		if(key[KEY_DOWN] || key[KEY_2_PAD]){
			camy += CAMERA_SPEED;
		}
		
		if(key[KEY_PGDN]){
			camera_zoom -= CAMERA_ZOOM_SPEED;
			if(camera_zoom<1.0) camera_zoom=1.0;
		}
		if(key[KEY_PGUP]){
			camera_zoom += CAMERA_ZOOM_SPEED;
			if(camera_zoom>15.0) camera_zoom=15.0;
		}
		if(key[KEY_SPACE]){
			camera_zoom = CAMERA_ZOOM;
		}
		
		if(key[KEY_S]){
			if(!follow_flag){
				follow_flag = 1;
				follow++;
				if(follow>=balls) follow = -1;
			}
		}else if(key[KEY_X]){
			if(!follow_flag){
				follow_flag = 1;
				follow--;
				if(follow<-1) follow = balls-1;
			}
		}else{
			follow_flag = 0;
		}
		if(key[KEY_W]) follow = -2;
		
		if(mouse_b&1){
			if(!click_flag){
				click_flag = 1;
				click_x = mouse_x;
				click_y = mouse_y;
			}
		}else{
			click_flag = 0;
		}
		
		count_down();
		move();
		
		if(follow>=0){
			camx = b[follow].x*zoom;
			camy = b[follow].y*zoom;
		}else if(follow==-1){
			camx = b[fastest].x*zoom;
			camy = b[fastest].y*zoom;
		}
		
		if(camx<-HALF_X) camx += HALF_X*2;
		if(camx>=HALF_X) camx -= HALF_X*2;
		if(camy<-HALF_Y) camy += HALF_Y*2;
		if(camy>=HALF_Y) camy -= HALF_Y*2;
		
		clear(buffer);
		show();
		show_mini();
		//Mostrar frame:
		blit(buffer, screen, 0, 0, 0, 0, HALF_X*2, HALF_Y*2);
		smart_rest(15);
		iteration++;
	}
	
	destroy_bitmap(buffer);
	allegro_exit();
	return 0;
}
END_OF_MAIN();

int my_random(int min, int max){
	return min+rand()%(max-min+1);
}

Chunk *get_pos_chunk(float x, float y){
	int i,j;
	
	i = (y+HALF_Y/zoom)/chunk_width;
	j = (x+HALF_X/zoom)/chunk_height;
	
	while(j<0) j+=cx;
	while(j>=cx) j-=cx;
	while(i<0) i+=cy;
	while(i>=cy) i-=cy;
	
	return chunk[i][j];
}

void move_to_chunk(Chunk *chunk, Ball *ball){
	if(ball->chunk){
		if(ball->node->next){
			ball->node->next->prev = ball->node->prev;
			ball->node->prev->next = ball->node->next;
		}else{
			ball->node->prev->next = NULL;
		}
	}
	
	ball->node->next = chunk->list.head->next;
	ball->node->prev = chunk->list.head;
	ball->node->prev->next = ball->node;
	if(ball->node->next) ball->node->next->prev = ball->node;
	ball->chunk = chunk;
}

void create_chunks(void){
	int i,j;
	cx = ceil(2*HALF_X/(zoom*CHUNK_SIZE));
	cy = ceil(2*HALF_Y/(zoom*CHUNK_SIZE));
	
	chunk_width = 2*HALF_X/(zoom*cx);
	chunk_height = 2*HALF_Y/(zoom*cy);
	
	int bx = ceil((float)cx/BARRIER_DIST);
	int by = ceil((float)cy/BARRIER_DIST);
	barrier_width = cx/bx;
	barrier_height = cy/by;
	
	//create chunks
	chunk = malloc(cy*sizeof(Chunk**));
	for(i=0; i<cy; i++){
		chunk[i] = malloc(cx*sizeof(Chunk*));
		for(j=0; j<cx; j++){
			chunk[i][j] = malloc(sizeof(Chunk));
			chunk[i][j]->min.x = -HALF_X/zoom + j*chunk_width;
			chunk[i][j]->min.y = -HALF_Y/zoom + i*chunk_height;
			chunk[i][j]->max.x = chunk[i][j]->min.x + chunk_width;
			chunk[i][j]->max.y = chunk[i][j]->min.y + chunk_height;
			if(chunk[i][j]->max.x > HALF_X/zoom) chunk[i][j]->max.x = HALF_X/zoom;
			if(chunk[i][j]->max.y > HALF_Y/zoom) chunk[i][j]->max.y = HALF_Y/zoom;
			chunk[i][j]->list.head = malloc(sizeof(BallNode));
			chunk[i][j]->list.head->prev = NULL;
			chunk[i][j]->list.head->next = NULL;
			chunk[i][j]->list.head->ball = NULL;
		}
	}
	
	//create relationships
	//right
	for(i=0; i<cy; i++){
		for(j=0; j<cx-1; j++){
			chunk[i][j]->right = chunk[i][j+1];
		}
		chunk[i][cx-1]->right = chunk[i][0];
	}
	//left
	for(i=0; i<cy; i++){
		for(j=1; j<cx; j++){
			chunk[i][j]->left = chunk[i][j-1];
		}
		chunk[i][0]->left = chunk[i][cx-1];
	}
	//down
	for(j=0; j<cx; j++){
		for(i=0; i<cy-1; i++){
			chunk[i][j]->down = chunk[i+1][j];
		}
		chunk[cy-1][j]->down = chunk[0][j];
	}
	//up
	for(j=0; j<cx; j++){
		for(i=1; i<cy; i++){
			chunk[i][j]->up = chunk[i-1][j];
		}
		chunk[0][j]->up = chunk[cy-1][j];
	}
	
	//move balls to chunks
	for(i=0; i<balls; i++){
		move_to_chunk(get_pos_chunk(b[i].x, b[i].y), &b[i]);
	}
}

void move(void){
	int i;
	int chunk_x, chunk_y;
	float v = 0;
	
	for(i=0; i<balls; i++){
		b[i].vx *= INERTIA;
		b[i].vy *= INERTIA;
		b[i].x += b[i].vx;
		b[i].y += b[i].vy;
		while(b[i].x<-HALF_X/zoom) b[i].x += HALF_X/zoom*2;
		while(b[i].x>=HALF_X/zoom) b[i].x += -HALF_X/zoom*2;
		while(b[i].y<-HALF_Y/zoom) b[i].y += HALF_Y/zoom*2;
		while(b[i].y>=HALF_Y/zoom) b[i].y += -HALF_Y/zoom*2;
		
		if(b[i].x<b[i].chunk->min.x || b[i].y<b[i].chunk->min.y || b[i].x>=b[i].chunk->max.x || b[i].y>=b[i].chunk->max.y){
			if(barriers && !(rand()%100<PERMEABILITY) && iteration>=BARRIER_START){
				//X bounce
				chunk_x = (b[i].x+HALF_X/zoom)/chunk_height;
				if(chunk_x%barrier_width==0 && b[i].x<b[i].chunk->min.x){
					b[i].x = 2*b[i].chunk->min.x - b[i].x;
					b[i].vx *= -1;
				}else if(chunk_x%barrier_width==barrier_width-1 && b[i].x>=b[i].chunk->max.x){
					b[i].x = 2*b[i].chunk->max.x - b[i].x;
					b[i].vx *= -1;
				}
				
				//Y bounce
				chunk_y = (b[i].y+HALF_Y/zoom)/chunk_width;
				if(chunk_y%barrier_height==0 && b[i].y<b[i].chunk->min.y){
					b[i].y = 2*b[i].chunk->min.y - b[i].y;
					b[i].vy *= -1;
				}else if(chunk_y%barrier_height==barrier_height-1 && b[i].y>=b[i].chunk->max.y){
					b[i].y = 2*b[i].chunk->max.y - b[i].y;
					b[i].vy *= -1;
				}
			}
			move_to_chunk(get_pos_chunk(b[i].x, b[i].y), &b[i]);
		}
		
		if(b[i].vx*b[i].vx + b[i].vy*b[i].vy>v){
			v = b[i].vx*b[i].vx + b[i].vy*b[i].vy;
			fastest = i;
		}
	}
}

void trail(float x, float y, float vx, float vy, int color, int lx, int ly){
	V3D_f v[3];
	float dist;
	int i;
	
	dist = pow(pow(vx, 2.0) + pow(vy, 2.0), 0.5);
	v[0].x = (x - TRAIL_SIZE*vx)*zoom - camx;
	v[0].y = (y - TRAIL_SIZE*vy)*zoom - camy;
	v[0].c = 0;
	v[1].x = (x + vy*BALL_RADIUS/dist)*zoom - camx;
	v[1].y = (y - vx*BALL_RADIUS/dist)*zoom - camy;
	v[1].c = color;
	v[2].x = (x - vy*BALL_RADIUS/dist)*zoom - camx;
	v[2].y = (y + vx*BALL_RADIUS/dist)*zoom - camy;
	v[2].c = color;
	
	for(i=0; i<3; i++){
		if(lx==1) v[i].x += HALF_X*2;
		if(ly==1) v[i].y += HALF_Y*2;
		if(lx==2) v[i].x -= HALF_X*2;
		if(ly==2) v[i].y -= HALF_Y*2;
		v[i].x *= camera_zoom;
		v[i].y *= camera_zoom;
		v[i].x += HALF_X;
		v[i].y += HALF_Y;
	}
	
	triangle3d_f(buffer, POLYTYPE_GRGB, buffer, &v[0], &v[1], &v[2]);
}

void show(void){
	int i, color, lx, ly, cx1, cx2, cy1, cy2, click_count=0, hits=0;
	float x, y, minx, miny, maxx, maxy;
	minx = -HALF_X*(camera_zoom-1)/2;
	miny = -HALF_Y*(camera_zoom-1)/2;
	maxx = HALF_X*(camera_zoom+3)/2;
	maxy = HALF_Y*(camera_zoom+3)/2;
	
	cx1 = min(click_x, mouse_x);
	cx2 = max(click_x, mouse_x);
	cy1 = min(click_y, mouse_y);
	cy2 = max(click_y, mouse_y);
	
	for(i=0; i<balls; i++){
		lx = 0;
		ly = 0;
		x = (b[i].x*zoom - camx)*camera_zoom + HALF_X;
		y = (b[i].y*zoom - camy)*camera_zoom + HALF_Y;
		if(x<minx){
			x += HALF_X*2*camera_zoom;
			lx = 1;
		}
		if(y<miny){
			y += HALF_Y*2*camera_zoom;
			ly = 1;
		}
		if(x>=maxx){
			x -= HALF_X*2*camera_zoom;
			lx = 2;
		}
		if(y>=maxy){
			y -= HALF_Y*2*camera_zoom;
			ly = 2;
		}
		color = 0x10000*b[i].red+0x100*b[i].green+b[i].blue+0x808080;
		trail(b[i].x, b[i].y, b[i].vx, b[i].vy, color, lx, ly);
		circlefill(buffer, x, y, BALL_RADIUS*zoom*camera_zoom, color);
		
		if(x>=cx1 && x<cx2 && y>=cy1 && y<cy2){
			click_count++;
			hits += b[i].hits;
		}
	}
	
	//mouse
	circlefill(buffer, mouse_x, mouse_y, 2.0, 0xFFFFFF);
	
	if(click_flag){
		rect(buffer, cx1, cy1, cx2, cy2, 0xFFFFFF);
		textprintf_centre_ex(buffer, font, (cx1+cx2)/2, (cy1+cy2)/2-5, 0xFFFFFF, -1, "Particles: %d", click_count);
		textprintf_centre_ex(buffer, font, (cx1+cx2)/2, (cy1+cy2)/2+5, 0xFFFFFF, -1, "Hits: %.2f", click_count==0? 0 : (float)hits/click_count);
	}
}

void show_mini(void){
	int i, color;
	float x, y, minx, miny, maxx, maxy, dx, dy;
	
	dx = 2*HALF_X*MINI_PROP;
	dy = 2*HALF_Y*MINI_PROP;
	maxx = 2*HALF_X-1;
	maxy = 2*HALF_Y-1;
	minx = maxx - dx;
	miny = maxy - dy;
	
	rectfill(buffer, minx, miny, maxx, maxy, 0);
	rect(buffer, minx, miny, maxx, maxy, 0xFFFFFF);
	rect(buffer, (camx + HALF_X - HALF_X/camera_zoom)*MINI_PROP+minx, (camy + HALF_Y - HALF_Y/camera_zoom)*MINI_PROP+miny, (camx + HALF_X + HALF_X/camera_zoom)*MINI_PROP+minx, (camy + HALF_Y + HALF_Y/camera_zoom)*MINI_PROP+miny, 0xFFFFFF);
	
	for(i=0; i<balls; i++){
		x = (b[i].x*zoom + HALF_X)*MINI_PROP + minx;
		y = (b[i].y*zoom + HALF_Y)*MINI_PROP + miny;
		
		color = 0x10000*b[i].red+0x100*b[i].green+b[i].blue+0x808080;
		putpixel(buffer, x, y, color);
	}
}

int explode(Chunk *chunk, float x, float y, int red, int green, int blue, float chunk_modifier){
	int ci, cj, d1, d2, d3, hits=0, chunk_dist;
	float dx, dy, dist, newdx, newdy;
	BallNode *node;
	Ball *ball;
	
	chunk_modifier = max(1, chunk_modifier/40);
	chunk_dist = min(min(cx, cy), chunk_modifier*CHUNK_DIST);
	for(ci=0; ci<chunk_dist; ci++){
		chunk = chunk->left->up;
	}
	
	for(ci=0; ci<2*chunk_dist+1; ci++){
		for(cj=0; cj<2*chunk_dist+1; cj++){
			node = chunk->list.head->next;
			while(node){
				ball = node->ball;
				
				dx = ball->x-x;
				dy = ball->y-y;
				if(dx<-HALF_X/zoom) dx += HALF_X/zoom*2;
				if(dx>=HALF_X/zoom) dx += -HALF_X/zoom*2;
				if(dy<-HALF_Y/zoom) dy += HALF_Y/zoom*2;
				if(dy>=HALF_Y/zoom) dy += -HALF_Y/zoom*2;
				dist = pow(pow(dx, 2.0)+pow(dy, 2.0), 0.5);
				if(dist>0.0 && dist<MAX_DISTANCE){
					d1 = red-ball->green;
					d2 = green-ball->blue;
					d3 = blue-ball->red;
					if(d1>=0x20) d1 -= 0x40;
					if(d1<-0x20) d1 += 0x40;
					if(d2>=0x20) d2 -= 0x40;
					if(d2<-0x20) d2 += 0x40;
					if(d3>=0x20) d3 -= 0x40;
					if(d3<-0x20) d3 += 0x40;
					d1 *= 2;
					d2 *= 2;
					d3 *= 2;
					
					if(d3>=0){
						d3 = pow(d3/0x40, 2.0)*PI/2;
					}else{
						d3 = -pow(d3/0x40, 2.0)*PI/2;
					}
					newdx = cos(d3)*dx + sin(d3)*dy;
					newdy = cos(d3+PI/2)*dx + sin(d3+PI/2)*dy;
					
					ball->vx += d1*EXPLOSION_FORCE*newdx/pow(dist, 2.0);
					ball->vy += d1*EXPLOSION_FORCE*newdy/pow(dist, 2.0);
					
					if(dist<EXPLOSION_RADIUS+BALL_RADIUS){
						if(d2>0){
							ball->time = EXPLOSION_TIME*my_random(7,13)/10;
							ball->red = red;
							ball->green = green;
							ball->blue = blue;
						}
						hits++;
					}
				}
				
				node = node->next;
			}
			
			chunk = chunk->right;
		}
		
		for(cj=0; cj<2*chunk_dist+1; cj++) chunk = chunk->left;
		chunk = chunk->down;
	}
	
	return hits;
}

void count_down(void){
	int i, j, explosions=0;
	for(i=0; i<balls && explosions<MAX_EXPLOSIONS; i++){
		b[i].time--;
		if(b[i].time<=0){
			explosions++;
			b[i].hits = explode(b[i].chunk, b[i].x, b[i].y, b[i].red, b[i].green, b[i].blue, b[i].hits);
			b[i].time = EXPLOSION_TIME*my_random(7,13)/10;
			if(!(rand()%MUTATION_CHANCE)){
				if(desvio<=MIN_DIVERSITY){
					b[i].red += my_random(-MUTATION_RATE, MUTATION_RATE);
					b[i].green += my_random(-MUTATION_RATE, MUTATION_RATE);
					b[i].blue += my_random(-MUTATION_RATE, MUTATION_RATE);
					if(b[i].red>=0x80) b[i].red -= 0x80;
					if(b[i].red<0) b[i].red += 0x80;
					if(b[i].green>=0x80) b[i].green -= 0x80;
					if(b[i].green<0) b[i].green += 0x80;
					if(b[i].blue>=0x80) b[i].blue -= 0x80;
					if(b[i].blue<0) b[i].blue += 0x80;
				}else{
					b[i].red = rand()%0x80;
					b[i].green = rand()%0x80;
					b[i].blue = rand()%0x80;
				}
			}
		}
	}
	//printf("%d\n", explosions);
}

void smart_rest(int duration){
	static int time=0;
	duration -= clock()-time;
	if(duration>0) rest(duration);
	time = clock();
}
