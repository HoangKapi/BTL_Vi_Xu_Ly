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

// C?p nh?t l?i logic hi?n th? ?? in ra các hàm toán h?c ??p m?t
void refresh_display(char *infix, int idx) {
	lcd_command(0x01);
	for(int i = 0; i < idx; i++) {
		if(infix[i] == 'S') lcd_puts("sqrt");
		else if(infix[i] == 'A') lcd_puts("abs");
		else if(infix[i] == 'L') lcd_puts("log");
		else if(infix[i] == 'l') lcd_puts("log"); // log c? s? tùy ch?nh a log b
		else lcd_char(infix[i]);
	}
}

// Hàm h? tr? format chu?i s? ?? lo?i b? các s? 0 th?a (vd: 5.00 -> 5)
void format_result(float val, char *buf) {
	dtostrf(val, 0, 3, buf);
	int len = strlen(buf);
	if(strchr(buf, '.')) {
		while(len > 0 && buf[len-1] == '0') {
			buf[len-1] = '\0';
			len--;
		}
		if(len > 0 && buf[len-1] == '.') {
			buf[len-1] = '\0';
		}
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
	if(op=='^'||op=='l') return 3; // 'l' là a log b (Toán t? 2 ngôi)
	if(op=='S'||op=='A'||op=='L') return 4; // Các hàm 1 ngôi ?u tiên cao nh?t
	return 0;
}

void infixToPostfix(char *infix, char *postfix){
	StackC s; initC(&s);
	int i=0, k=0;
	int last_was_op = 1;

	while(infix[i] != '\0'){
		char c = infix[i];
		if (c == '-' && last_was_op) {
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
		else {
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
			if(op == 'S' || op == 'A' || op == 'L'){ // Toán t? 1 ngôi
				float a = popF(&s);
				if(error_flag) return 0;

				if(op == 'S'){
					if(a < 0) { error_flag = 2; return 0; }
					pushF(&s, sqrt(a));
				}
				else if(op == 'A'){
					pushF(&s, fabs(a));
				}
				else if(op == 'L'){
					if(a <= 0) { error_flag = 2; return 0; }
					pushF(&s, log10(a));
				}
			}
			else { // Toán t? 2 ngôi (+, -, *, /, ^, l)
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
					break;
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
	switch(PINB & 0x1F){ case 0x1E: return '4'; case 0x1D: return '9'; case 0x1B: return 'S'; case 0x17: return 'C'; case 0x0F: return 'l';}
	PORTD = 0xFD; _delay_us(10);
	switch(PINB & 0x1F){ case 0x1E: return '3'; case 0x1D: return '8'; case 0x1B: return '/'; case 0x17: return 'D'; case 0x0F: return 'A';}
	PORTD = 0xFB; _delay_us(10);
	switch(PINB & 0x1F){ case 0x1E: return '2'; case 0x1D: return '7'; case 0x1B: return '*'; case 0x17: return '.'; case 0x0F: return 'L';}
	PORTD = 0xF7; _delay_us(10);
	switch(PINB & 0x1F){ case 0x1E: return '1'; case 0x1D: return '6'; case 0x1B: return '-'; case 0x17: return '='; case 0x0F: return ')';}
	PORTD = 0xEF; _delay_us(10);
	switch(PINB & 0x1F){ case 0x1E: return '0'; case 0x1D: return '5'; case 0x1B: return '+'; case 0x17: return '^'; case 0x0F: return '(';}
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

// ================= MAIN =================
int main(void) {
	char infix[100], postfix[100], res_str[16];
	int idx = 0;
	char key;
	
	int waiting_for_ans = 0;
	float ans_val = 0.0;

	DDRB = 0x00; PORTB = 0xFF; // Inputs
	DDRD = 0xFF; DDRC = 0xFF; // Outputs
	lcd_init();

	while(1){
		key = GET_KEY();
		
		// Kích ho?t tính n?ng ANS n?u ?ang có k?t qu?
		if(waiting_for_ans) {
			lcd_command(0x01);
			waiting_for_ans = 0;
			idx = 0;
			
			// N?u phím b?m là 1 toán t? 2 ngôi, t? ??ng chèn k?t qu? c? vào
			if (!error_flag && (key == '+' || key == '-' || key == '*' || key == '/' || key == '^' || key == 'l')) {
				format_result(ans_val, res_str);
				for(int i = 0; res_str[i] != '\0'; i++) {
					infix[idx++] = res_str[i];
				}
				infix[idx++] = key; // Chèn ti?p toán t? v?a b?m
				refresh_display(infix, idx);
				continue;
			}
			// N?u b?m C thì ch? c?n gi? idx = 0 (t?o m?i hoàn toàn)
			else if (key == 'C') {
				continue;
			}
			// N?u b?m phím khác (s?, hàm...), nó s? t? r?t xu?ng logic bên d??i ?? x? lý bình th??ng
		}

		if(key == '='){
			if(idx == 0) continue;
			infix[idx] = '\0';
			refresh_display(infix, idx); lcd_char('=');
			
			error_flag = 0;
			infixToPostfix(infix, postfix);
			ans_val = evalPostfix(postfix);

			lcd_command(0xC0); // Dòng 2
			if(error_flag == 1) lcd_puts("Syntax Error");
			else if(error_flag == 2) lcd_puts("Math Error");
			else {
				format_result(ans_val, res_str);
				lcd_puts(res_str);
			}
			waiting_for_ans = 1; // B?t c? ch? cho l?n nh?n ti?p theo
		}
		else if(key == 'C'){ idx = 0; lcd_command(0x01); }
		else if(key == 'D'){
			if(idx > 0) { idx--; refresh_display(infix, idx); }
		}
		// X? lý thông minh: Nh?n L, A, S t? ??ng thêm d?u '('
		else if(key == 'S' || key == 'A' || key == 'L'){
			if(idx < 98) {
				infix[idx++] = key;
				infix[idx++] = '(';
				refresh_display(infix, idx);
			}
		}
		else {
			if(idx < 99) {
				infix[idx++] = key;
				refresh_display(infix, idx);
			}
		}
	}
}