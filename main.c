#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// LCD Connections (4-bit mode)
#define LCD_PORT PORTC
#define LCD_DDR  DDRC
#define RS 0
#define EN 2
#define PI 3.14159265

int error_flag = 0;

// ================= LCD FUNCTIONS =================
void lcd_command(unsigned char cmnd) {
	LCD_PORT = (LCD_PORT & 0x0F) | (cmnd & 0xF0);
	LCD_PORT &= ~(1 << RS);
	LCD_PORT |= (1 << EN);
	_delay_us(1);
	LCD_PORT &= ~(1 << EN);
	_delay_us(200);
	LCD_PORT = (LCD_PORT & 0x0F) | (cmnd << 4);
	LCD_PORT |= (1 << EN);
	_delay_us(1);
	LCD_PORT &= ~(1 << EN);
	_delay_ms(2);
}

void lcd_char(unsigned char data) {
	LCD_PORT = (LCD_PORT & 0x0F) | (data & 0xF0);
	LCD_PORT |= (1 << RS);
	LCD_PORT |= (1 << EN);
	_delay_us(1);
	LCD_PORT &= ~(1 << EN);
	LCD_PORT = (LCD_PORT & 0x0F) | (data << 4);
	LCD_PORT |= (1 << EN);
	_delay_us(1);
	LCD_PORT &= ~(1 << EN);
	_delay_ms(2);
}

void lcd_init() {
	LCD_DDR = 0xFF;
	_delay_ms(20);
	lcd_command(0x02); // 4-bit mode
	lcd_command(0x28); // 2 line, 5x7 matrix
	lcd_command(0x0C); // Display on, cursor off
	lcd_command(0x01); // Clear
}

void lcd_puts(char *s) {
	while(*s) lcd_char(*s++);
}

// Hàm v? l?i toàn b? màn hình t? m?ng infix
void refresh_display(char *infix, int idx) {
	lcd_command(0x01);
	for(int i = 0; i < idx; i++) {
		if(infix[i] == 'S') lcd_puts("sqrt");
		else if(infix[i] == 'A') lcd_puts("abs");
		else if(infix[i] == 'L') lcd_puts("log");
		else if(infix[i] == 'l') lcd_puts("log_");
		else if(infix[i] == 's') lcd_puts("sin");
		else if(infix[i] == 'c') lcd_puts("cos");
		else if(infix[i] == 't') lcd_puts("tan");
		else lcd_char(infix[i]);
	}
}

// ================= STACK LOGIC =================
#define MAX 50
typedef struct { float data[MAX]; int top; } StackF;
typedef struct { char data[MAX]; int top; } StackC;

void initF(StackF *s){ s->top = -1; }
void initC(StackC *s){ s->top = -1; }
void pushF(StackF *s, float x){ if(s->top < MAX-1) s->data[++s->top] = x; }
float popF(StackF *s){ if(s->top < 0) { error_flag = 1; return 0; } return s->data[s->top--]; }
void pushC(StackC *s, char x){ if(s->top < MAX-1) s->data[++s->top] = x; }
char popC(StackC *s){ if(s->top < 0) { error_flag = 1; return 0; } return s->data[s->top--]; }
char peekC(StackC *s){ return (s->top < 0) ? 0 : s->data[s->top]; }
int isEmptyC(StackC *s){ return s->top == -1; }

// ================= MATH LOGIC =================
int priority(char op){
	if(op=='+'||op=='-') return 1;
	if(op=='*'||op=='/') return 2;
	if(op=='^' || op == 'l') return 3;
	if(op=='S' || op=='A' || op=='L' || op=='!' || op=='c' || op=='s' || op=='t') return 4;
	return 0;
}

float factorial(float n) { // hàm tính giai th?a
	if (n < 0) { error_flag = 2; return 0; }
	int num = (int)n;
	float res = 1.0;
	for (int i = 1; i <= num; i++) res *= i;
	return res;
}

void infixToPostfix(char *infix, char *postfix){
	StackC s; initC(&s);
	int i=0, k=0;
	int last_was_op = 1;

	while(infix[i] != '\0'){
		char c = infix[i];
		if (c == '-' && last_was_op) { // S? âm
			postfix[k++] = '-';
			i++;
			while((infix[i]>='0' && infix[i]<='9') || infix[i]=='.') postfix[k++] = infix[i++];
			postfix[k++] = ' ';
			last_was_op = 0;
		}
		else if((c >= '0' && c <= '9') || c == '.'){
			while((infix[i]>='0' && infix[i]<='9') || infix[i]=='.') postfix[k++] = infix[i++];
			postfix[k++] = ' ';
			last_was_op = 0;
		}
		else if(c == '('){ pushC(&s, '('); i++; last_was_op = 1; }
		else if(c == ')'){
			while(!isEmptyC(&s) && peekC(&s) != '(') { postfix[k++] = popC(&s); postfix[k++] = ' '; }
			popC(&s); i++; last_was_op = 0;
		}
		else { // Toán t? +, -, *, /, ^, S, L, A, l, s, c, t, !
			while(!isEmptyC(&s) && priority(peekC(&s)) >= priority(c)){
				postfix[k++] = popC(&s); postfix[k++] = ' ';
			}
			pushC(&s, c); i++; last_was_op = 1;
		}
	}
	while(!isEmptyC(&s)){ postfix[k++] = popC(&s); postfix[k++] = ' '; }
	postfix[k] = '\0';
}

float evalPostfix(char *postfix){
	StackF s; initF(&s);
	char *token = strtok(postfix, " ");
	while(token != NULL){
		if((token[0] >= '0' && token[0] <= '9') || (token[0] == '-' && strlen(token) > 1) || token[0] == '.'){
			pushF(&s, atof(token));
			} else {
			char op = token[0];
			if(op == 'S' || op == 'A' || op == 'L' || op == 'c' || op == 's' || op == 't' || op == '!'){ // Toán t? 1 ngôi
				float a = popF(&s);
				if(error_flag) return 0;

				if(op == 'S'){
					if(a < 0) { error_flag = 2; return 0; }
					pushF(&s, sqrt(a));
				}
				else if(op == 'A'){
					pushF(&s, fabs(a)); // tr? tuy?t ??i
				}
				else if(op == 'L'){
					if(a <= 0) { error_flag = 2; return 0; }
					pushF(&s, log10(a)); // log t? nhiên
				}
				else if(op == '!'){
					pushF(&s, factorial(a));
				}
				else if(op == 's'){
					pushF(&s, sin(a * PI / 180.0));
				}
				else if(op == 'c'){
					pushF(&s, cos(a * PI / 180.0));
				}
				else if(op == 't'){
					pushF(&s, tan(a * PI / 180.0));
				}
			}
			else { // Toán t? 2 ngôi
				float b = popF(&s);
				float a = popF(&s);
				if(error_flag) return 0;
				switch(op){
					case '+': pushF(&s, a+b); break;
					case '-': pushF(&s, a-b); break;
					case '*': pushF(&s, a*b); break;
					case '/': if(b==0) { error_flag = 2; return 0; } pushF(&s, a/b); break;
					case '^': pushF(&s, pow(a,b)); break;
					case 'l':
					// a log b (log c? s? a c?a b) = ln(b) / ln(a)
					if(a <= 0 || a == 1 || b <= 0) { error_flag = 2; return 0; }
					pushF(&s, log(b) / log(a));
				}
			}
		}
		token = strtok(NULL, " ");
	}
	return popF(&s);
}

// ================= KEYPAD FUNCTIONS =================
char GET_KEY_RAW(void) {
	PORTD = 0xFE; _delay_us(10);
	switch(PINB & 0x3F){ case 0x3E: return '4'; case 0x3D: return '9'; case 0x3B: return 'S'; case 0x37: return 'C'; case 0x2F: return 'M';} // Thay phím l b?ng M
	PORTD = 0xFD; _delay_us(10);
	switch(PINB & 0x3F){ case 0x3E: return '3'; case 0x3D: return '8'; case 0x3B: return '/'; case 0x37: return 'D'; case 0x2F: return 'A'; case 0x1F: return '!';}
	PORTD = 0xFB; _delay_us(10);
	switch(PINB & 0x3F){ case 0x3E: return '2'; case 0x3D: return '7'; case 0x3B: return '*'; case 0x37: return '.'; case 0x2F: return '!'; case 0x1F: return 't';} // Thay phím L b?ng !
	PORTD = 0xF7; _delay_us(10);
	switch(PINB & 0x3F){ case 0x3E: return '1'; case 0x3D: return '6'; case 0x3B: return '-'; case 0x37: return '='; case 0x2F: return ')'; case 0x1F: return 'c';}
	PORTD = 0xEF; _delay_us(10);
	switch(PINB & 0x3F){ case 0x3E: return '0'; case 0x3D: return '5'; case 0x3B: return '+'; case 0x37: return '^'; case 0x2F: return '('; case 0x1F: return 's';}
	return 0;
}

char GET_KEY(void) {
	char key, key2;
	do { key = GET_KEY_RAW(); } while(key == 0);
	_delay_ms(20);
	key2 = GET_KEY_RAW();
	if(key != key2) return 0;
	while(GET_KEY_RAW() != 0);
	return key;
}

int main(void) {
	char infix[100], postfix[100], res_str[16];
	int idx = 0;
	char key;

	DDRB = 0x00; PORTB = 0xFF; // Inputs
	DDRD = 0xFF; DDRC = 0xFF; // Outputs
	lcd_init();

	while(1){
		key = GET_KEY();
		if(key == '='){
			if(idx == 0) continue;
			infix[idx] = '\0';
			refresh_display(infix, idx); lcd_char('=');
			
			error_flag = 0;
			infixToPostfix(infix, postfix);
			float result = evalPostfix(postfix);

			lcd_command(0xC0); // Dòng 2
			if(error_flag == 1) lcd_puts("Syntax Error");
			else if(error_flag == 2) lcd_puts("Math Error");
			else {
				dtostrf(result, 8, 2, res_str);
				lcd_puts(res_str);
			}
			while(GET_KEY() != 'C'); // ??i nh?n C
			idx = 0; lcd_command(0x01);
		}
		else if(key == 'C'){ idx = 0; lcd_command(0x01); }
		else if(key == 'D'){
			if(idx > 0) { idx--; refresh_display(infix, idx); }
		}
		else if(key == 'M') {
			lcd_command(0x01);
			lcd_puts("1:TRIG  2:LOG");
			
			char menu_choice = GET_KEY();
			if(menu_choice == '1') {
				lcd_command(0x01);
				lcd_puts("1:sin 2:cos 3:tan");
				char trig_choice = GET_KEY();
				if(trig_choice == '1' && idx < 98) { infix[idx++] = 's'; infix[idx++] = '('; }
				else if(trig_choice == '2' && idx < 98) { infix[idx++] = 'c'; infix[idx++] = '('; }
				else if(trig_choice == '3' && idx < 98) { infix[idx++] = 't'; infix[idx++] = '('; }
			}
			else if(menu_choice == '2') {
				lcd_command(0x01);
				lcd_puts("1:log(x) 2:log_ab");
				char log_choice = GET_KEY();
				if(log_choice == '1' && idx < 98) { infix[idx++] = 'L'; infix[idx++] = '('; }
				else if(log_choice == '2' && idx < 99) { infix[idx++] = 'l'; }
			}
			refresh_display(infix, idx);
		}
		else {
			if(idx < 99) {
				if(key == 'S' || key == 'A' || key == 'L' || key == 's' || key == 'c' || key == 't') {
					if(idx < 98) {
						infix[idx++] = key;
						infix[idx++] = '(';
						refresh_display(infix, idx);
					}
				}
				else {
					infix[idx++] = key;
					refresh_display(infix, idx);
				}
			}
		}
	}
}