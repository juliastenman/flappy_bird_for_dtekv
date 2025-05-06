#define PIPES_TO_GENERATE 1000
// check that code above works

/* Below functions can be found in other files. */
extern void enable_interrupt(void);
extern void display_string(char*);
extern char* time2string(int time);
extern void delay(int);

volatile char *VGA = (volatile char*) 0x08000000;
volatile int *VGA_CTRL = (volatile int*) 0x04000100;
volatile int *button_ptr = (volatile int*) 0x040000d0;
volatile unsigned char *display_ptr = (unsigned char*) 0x04000050;      // 1 byte per display
volatile unsigned int *switch_ptr = (unsigned int*) 0x04000010;     // 10 bits (2 bytes sufficient)
volatile unsigned short *timer_ptr = (unsigned short*) 0x04000020;      // 2 bytes per register


// screen size
const int WIDTH = 320;
const int HEIGHT = 240;
const int SIZE = WIDTH*HEIGHT;
const int SKY_HEIGHT = 200;

// main menu symbol
const int PLAY_WIDTH = 70;

// colors
const char BGR_COLOR = 0b00010011; 
const char GROUND_COLOR = 0b00010100;
const char BIRD_COLOR = 0b11100000;
const char PIPE_COLOR = 0b00001000;
const char PIPE_LIGHT_COLOR = 0b00101100;
const char PIPE_HIGHLIGHT = 0b00110101;
const char BEAK_COLOR = 0b1111100;

// bird pos.
const int BIRD_RADIUS = 7;
const int BIRD_POS_X = WIDTH/5;
const int BIRD_POS_X0 = BIRD_POS_X-BIRD_RADIUS;
const int BIRD_POS_X1 = BIRD_POS_X+BIRD_RADIUS;
const int BIRD_START_POS_Y = HEIGHT/2;

// pipes
const int PIPE_RADIUS=15;
const int PIPE_HORIZONTAL_SPACE = 130;
const int PIPE_HALF_GAP = 30;
const int PIPE_MIN_HEIGHT = 50;

// speeds
const int PIPE_SPEED = 3;
const int FALL_SPEED = 3;	// pixels down per frame
const int JUMP_SPEED = 7; 	// pixels up per frame
const int JUMP_TIME = 4; 	// frames for each jump

// ------------------------
// modified with interrupts:

// is modified after each tenth of a second
int timeoutcount = 0;

// is modified when jump
int jump_frames = 0;

int HIGH_SCORE_START = 0;
volatile int* high_score = &HIGH_SCORE_START;

int pipe_speed = PIPE_SPEED;
// --------------------------

enum GAME_STATUS {
	MAIN_MENU,
	GAME,
	GAME_OVER
};

enum GAME_STATUS status = MAIN_MENU;

// true if proceed to next screen
int proceed_val = 0;
volatile int* proceed = &proceed_val;


void setup_pipes(int (*pipes)[1000]) {
	for (int i=0; i<PIPES_TO_GENERATE;i++) {
		(*pipes)[i] = ((11111+7*(i+timeoutcount))/((i+1)%23)) % (SKY_HEIGHT - 2*PIPE_MIN_HEIGHT) + PIPE_MIN_HEIGHT;
	}
}

void setup_bg_ground() {
	for (int VGA_offset=0; VGA_offset<2;VGA_offset++) {
		// Ground
		for (int i=WIDTH*SKY_HEIGHT; i<SIZE;i++) {
			*(VGA+VGA_offset*SIZE+i) = GROUND_COLOR;
		}	
	}
}

/* Added function for setting display display_number to value. (DOT IS ADDED AND MAY NEED TO BE REMOVED) */
void set_displays(int display_number, int value, int dot) {
	// the section will light up if the bit is 0
	char numbers_on_display[] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 0xFF};

	volatile unsigned char* ptr = display_ptr + 16*display_number;
	int displaybits = numbers_on_display[value];
	if (dot) {
			displaybits -= 0x80;
	}
	*ptr = displaybits;
}

void setup_7seg_displays() {
	for (int i=0; i<6;i++) {
		set_displays(i,10,0);
	}
}

void set_score(int score) {
	int x = score % 10;
	int display = 0;
	while (score) {
		score /= 10;
		set_displays(display,x,0);
		x = score % 10;
		++display;
	}
}

void toggle_high_score_display() {
	static int toggle_high_score = 0;
	if (toggle_high_score) {
		set_score(*high_score);
	} else {
		setup_7seg_displays();
	}
	toggle_high_score = (toggle_high_score+1)%2;
}

void handle_interrupt(int mcause) {
	// button press
	static int button_press = 0;
	if (mcause == 18) {
		if (button_press == 0) {
			if (status == GAME) {
				jump_frames = JUMP_TIME;
			} else {
				*proceed = 1;
			}
			++button_press;
		} else {
			button_press = 0;
		}

		// button_ptr + 3 is the edge capture register 
		// (setting the bit for the button clears the edge capture)
		*(button_ptr+3) |= 0b1; 
		
	} 
	if (mcause == 17) {
		if (status == MAIN_MENU) {
			delay(50);
			toggle_high_score_display();
		}
		*(switch_ptr+3) |= 0b1;
	}
	if (mcause == 16) {
		*timer_ptr &= ~(0b1);   // reset timeout bit
		++timeoutcount;
		if (timeoutcount==600) {
			++pipe_speed;
			timeoutcount=0;
		}

	}
}

void labinit(void) {
	// button_ptr + 2 is the interrupt mask 
	// setting bit 1 allows button to interrupt
	*(button_ptr+2) = 0b1; 
	*(switch_ptr+2) = 0b1;
	*(timer_ptr+2*2) = 0xC6BF; // PERIODL
	*(timer_ptr+3*2) = 0x002D; // PERIODH (3 mil - 1 together)
	*(timer_ptr+1*2) |= 0b111; // CONTROL: start timer, let it continue and allow interrupts
	enable_interrupt();
}

void drawBackground(int VGA_offset) {
	// Sky
	for (int i=0; i<WIDTH*SKY_HEIGHT;i++) {
		*(VGA+VGA_offset*SIZE+i) = BGR_COLOR;
	}
}

// Bird, uses (BIRD_POS_X, bird_pos_y) as center
void drawBird(int bird_pos_y, int VGA_offset) {
	// highest position and lowest allowed y value on screen is 0 
	int y0=bird_pos_y-BIRD_RADIUS > 0 ? bird_pos_y-BIRD_RADIUS : 0;
	int y1=bird_pos_y+BIRD_RADIUS;

	// draw body
	for (int y=y0; y<y1;y++) {
		for (int x=BIRD_POS_X0; x<BIRD_POS_X1;x++) {
			*(VGA + VGA_offset*SIZE + y*WIDTH + x) = BIRD_COLOR;
		}
	}

	// draw wings
	for (int x=0; x<5; x++) {
		for (int y=0; y<5; y++) {
			if (bird_pos_y+y>=0) {
				if (jump_frames > 0) {
					if (1.5*y == -x+6 || 1.5*y -1 == -x+6) {
						*(VGA + VGA_offset*SIZE + (bird_pos_y+y)*WIDTH + (BIRD_POS_X0+2+x)) = 0b10000000;
				 	}
				} else {
					if (3*y-3 == x || 3*y-2==x) {
						*(VGA + VGA_offset*SIZE + (bird_pos_y+y)*WIDTH + (BIRD_POS_X0+2+x)) = 0b10000000;
				 	}
				}

			}
		}
	}

	// draw eye
	int y01 = y1-11 > 0 ? y1-11 : 0;
	for (int y=y01;y<y1-4;y++) {
		for (int x=BIRD_POS_X1-5; x<BIRD_POS_X1-1;x++) {
			if (x >= BIRD_POS_X1-4 && y >= y1-9 && !(x==BIRD_POS_X1-3 && y==y1-8)) {
				*(VGA + VGA_offset*SIZE + y*WIDTH + x) = 0;
			} else {
				*(VGA + VGA_offset*SIZE + y*WIDTH + x) = 0xFF;
			}
		}
	}
	// draw beak
	int y02 = y1 > 3 ? y1-3 : 0;
	int y03 = y1 > 4 ? y1-4 : 0; 
	if (y02>0) {
		*(VGA + VGA_offset*SIZE + (y02)*WIDTH + (BIRD_POS_X1-1)) = BEAK_COLOR;
		if (y03>0) {
			*(VGA + VGA_offset*SIZE + (y03)*WIDTH + (BIRD_POS_X1-1)) = BEAK_COLOR;
			*(VGA + VGA_offset*SIZE + (y03)*WIDTH + (BIRD_POS_X1)) = BEAK_COLOR;
		}
	}
}


void drawPipes(int pipes[], int pipe_idx, int first_pipe_center_x, int VGA_offset) {
	int idx = pipe_idx;
	int x_center = first_pipe_center_x;
	
	while (x_center - PIPE_RADIUS < WIDTH) {
		int x0 = x_center-PIPE_RADIUS > 0 ? x_center-PIPE_RADIUS : 0;
		int x1 = x_center+PIPE_RADIUS < WIDTH ? x_center+PIPE_RADIUS : WIDTH;
		
		int y_center = pipes[idx];

		// high part of pipe
		for (int y=0; y<y_center-PIPE_HALF_GAP; y++) {
			for (int x=x0; x<x1; x++) {
				if (x > x0 + 5 && x < x1-4) {
					if (x >= x1-10 && x < x1-7) {
						*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_HIGHLIGHT;
					} else {
						*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_LIGHT_COLOR;
					}
				} else {
					*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_COLOR;

				}
			}
		}

		// low part of pipe
		for (int y=y_center+PIPE_HALF_GAP; y<SKY_HEIGHT; y++) {
			for (int x=x0; x<x1; x++) {
				if (x > x0 + 5 && x < x1-4) {
					if (x >= x1-10 && x < x1-7) {
						*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_HIGHLIGHT;
					} else {
						*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_LIGHT_COLOR;
					}
				} else {
					*(VGA + VGA_offset*SIZE + y*WIDTH + x) = PIPE_COLOR;
				}
			}
		}

		++idx;
		x_center += PIPE_HORIZONTAL_SPACE;
	}
}

void game_over(int VGA_offset) {
	status = GAME_OVER;
	pipe_speed = PIPE_SPEED;
	timeoutcount=0;
	for (int i=0; i<SIZE;i++) {
		*(VGA+VGA_offset*SIZE+i) = BIRD_COLOR;
	}

	// sends vga address as output
	*(VGA_CTRL+1) = (unsigned int) VGA+VGA_offset*SIZE;
	*(VGA_CTRL+0) = 0;
	
	// wait until proceed
	while (!(*proceed)) {}
	*proceed = 0;
}


void game() {
	status = GAME;
	// offset for writing to first or second half of VGA buffer
	int VGA_offset = 1;
	int bird_pos_y = BIRD_START_POS_Y;
	int pipe_idx = 0;
	int first_pipe_center_x = WIDTH + PIPE_RADIUS;
	int score = 0;
	
	// stores all pipes
	int pipes[PIPES_TO_GENERATE];
	setup_pipes(&pipes);

	setup_bg_ground();
	setup_7seg_displays();
	set_displays(0,0,0);
	while (1) {
		// stall while swapping
		while (*(VGA_CTRL+3)&0b1) {
			asm volatile("nop");
		}

		drawBackground(VGA_offset);
		drawBird(bird_pos_y, VGA_offset);
		drawPipes(pipes,pipe_idx, first_pipe_center_x, VGA_offset);	


		// sends vga address as output
		*(VGA_CTRL+1) = (unsigned int) VGA+VGA_offset*SIZE;
		
		// swaps buffer and backbuffer
		*(VGA_CTRL+0) = 0;

		
		// // Small delay
		// for (int i = 0; i < 100000; i++) {
		// 	asm volatile("nop");
		// }
		delay(5);
		
		// swap side of vga buffer		
		VGA_offset = (VGA_offset+1)%2;

		// if leftmost visible pipe out of screen, make next pipe the first pipe
		if (first_pipe_center_x + PIPE_RADIUS < 0) {
			pipe_idx = (pipe_idx + 1)%1000;
			first_pipe_center_x += PIPE_HORIZONTAL_SPACE;
		}

		// move first pipe by speed
		first_pipe_center_x -= pipe_speed;

		if (first_pipe_center_x >= BIRD_POS_X - BIRD_RADIUS && first_pipe_center_x < BIRD_POS_X - BIRD_RADIUS + pipe_speed) {
			++score;
		}

		set_score(score);

		// positive y means downwards
		if (bird_pos_y + BIRD_RADIUS >= SKY_HEIGHT){
			break;
		}

		// if bird could collide with pipe on x-axis
		if (BIRD_POS_X+BIRD_RADIUS>=first_pipe_center_x-PIPE_RADIUS && BIRD_POS_X-BIRD_RADIUS<=first_pipe_center_x+PIPE_RADIUS) {
			if (bird_pos_y+BIRD_RADIUS>=pipes[pipe_idx]+PIPE_HALF_GAP || bird_pos_y-BIRD_RADIUS<=pipes[pipe_idx]-PIPE_HALF_GAP) {
				break;
			} 
		}

// 		bird's movement
		if (jump_frames > 0) {
			// display_string("jump frames is > 0\n");
			bird_pos_y -= JUMP_SPEED;
			jump_frames--;
		} else if (jump_frames == 0) {
			jump_frames--;
		} else {
			bird_pos_y += FALL_SPEED;
		}
	}
	if (score > *high_score) {
		*high_score = score;
	}
	game_over(VGA_offset);
}

void main_menu() {
	while (1) {
		status = MAIN_MENU;
		setup_7seg_displays();
		for (int i=0; i<SIZE;i++) {
			*(VGA+i) = BGR_COLOR;
		}

		volatile char* play_address = VGA+WIDTH*(HEIGHT/2-PLAY_WIDTH/2) - PLAY_WIDTH/2 +WIDTH/2;

		for (int y=0; y < 70; y++) {
			for (int x=0; x<70; x++) {
				if (2*y>x && 2*y < -x +140) {
					*(play_address + y*WIDTH + x) = 0xFF;
				}
				
			}
		}		

		// sends vga address as output
		*(VGA_CTRL+1) = (unsigned int) VGA;
		*(VGA_CTRL+0) = 0;
		
		while (!(*proceed)) {
			//
		}
		*proceed = 0;
		game();
	}
}

// for VGA 
int main(){
	labinit();
	main_menu();
}
