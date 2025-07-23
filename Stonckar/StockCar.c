#include <reg51.h>
#include <intrins.h>
#include <stdlib.h>
#include <Font_Header.h>


#define Data_Port P1
sbit RS = P2^0;
sbit RW = P2^1;
sbit E = P2^2;
sbit CS1 = P2^3;
sbit CS2 = P2^4;
sbit RST = P2^5;
sbit LINHA1 = P0^3;
sbit LINHA2 = P0^2;
sbit LINHA3 = P0^1;
sbit LINHA4 = P0^0;
sbit COL1 = P0^6;
sbit COL2 = P0^5;
sbit COL3 = P0^4;

// Configurações de elementos
#define LEFT_TRACK_BASE 34 // Lado esquerdo da pista inicial
#define RIGHT_TRACK_BASE 94 // Lado direito da pista inicial
#define TRACK_WIDTH 60 // Largura da pista
#define CAR_HEIGHT 1 //Altura do carro em páginas
#define CAR_WIDTH 6  // Largura do carro em pixels
#define MAX_OBSTACLES 3 // Número máximo de obstáculos simultâneos na pista
#define OBSTACLE_HEIGHT 1 //Altura dos obstáculos em páginas
#define OBSTACLE_WIDTH 6  //Largura dos obstáculos em pixels
#define SPACING 10 // Espaçamento entre obstáculos em pixels

// Variáveis globais 
bit em_curva=0, colisao=0;
int score = 0;
int size_curva, offset = 0, seta = 0;
char center_car = LEFT_TRACK_BASE + TRACK_WIDTH / 2;
signed char track_offsets[8];
int seta;


// Bitmaps de carro e obstáculos
const unsigned char car[6] = {0x7E, 0x7E, 0xFF, 0xFF, 0x7E, 0x7E};
const unsigned char enemy_car[6] = {0x7E, 0x7E, 0x81, 0x81, 0x7E, 0x7E};

// Função para comandos e dados
void GLCD_Write(char info, bit tipo) {
    Data_Port = info;
    RS = !tipo; 
		RW = 0; 
		E = 1; 
		_nop_();
		E = 0;
}

//Função de delay
void delay(unsigned int k) {
    while(k--) {
        unsigned char j = 112;
        while(j--);
    }
}

//Inicializaçao do display
void GLCD_Init() {
    CS1 = CS2 = RST = 1;
    delay(20);
    GLCD_Write(0x3E, 1); // Display OFF
    GLCD_Write(0xC0, 1); // Start line
    GLCD_Write(0x3F, 1); // Display ON
}

//Limpeza do display
void GLCD_ClearAll() {
    unsigned char page, col;
    for (page = 0; page < 8; page++) {
        CS1 = 0; CS2 = 1;
        GLCD_Write(0xB8 | page, 1);
        GLCD_Write(0x40, 1);
        for(col = 0; col < 64; col++) GLCD_Write(0, 0);
        
        CS1 = 1; CS2 = 0;
        GLCD_Write(0xB8 | page, 1);
        GLCD_Write(0x40, 1);
        for(col = 0; col < 64; col++) GLCD_Write(0, 0);
    }
    CS1 = CS2 = 1;
}

// Geração de curvas de forma aleatória
void Gerar_Curva() {
    int page;
    static unsigned char size_curva;
    
    // Lógica da curva
    if (!em_curva) {
        if (abs(offset) <= 1) {
            size_curva = (rand() %21) ;
            offset = (rand() & 1) ? 1 : -1;
            em_curva = 1;
        } else {
            offset += (offset > 0) ? -1 : 1;
        }
    } 
		else {
			if (--size_curva <= 0)
        em_curva = 0;
			offset += (offset > 0) ? 1 : -1;
    }
    
    // Desenho da pista
    for (page = 0; page < 8; page++) {
			int left_pos, right_pos;
        signed char po = (offset * (8 - page)) / 8;
        track_offsets[page] = po;
        
        left_pos = LEFT_TRACK_BASE + po;
        if (left_pos < 64) {
            CS1 = 0; CS2 = 1;
            GLCD_Write(0xB8 | page, 1);
            GLCD_Write(0x40 | left_pos, 1);
            GLCD_Write(0xFF, 0);
        }
        
        right_pos = RIGHT_TRACK_BASE + po;
        if (right_pos < 128) {
            CS1 = 1; CS2 = 0;
            GLCD_Write(0xB8 | page, 1);
            GLCD_Write(0x40 | (right_pos - 64), 1);
            GLCD_Write(0xFF, 0);
        }
    }
}

// Geração de obstáculos
typedef struct {
    unsigned char x, active, page;
} Obstacle;
Obstacle obstacles[MAX_OBSTACLES];
unsigned char obstacle_timer = 10;

void Generate_Obstacles() {
	//Obstáculos gerados com base no offset da curva
    static unsigned char timer = 10;
	unsigned char j;
	unsigned char i, col;
    if (--timer == 0) {
        timer = (rand() % 20);
        for (j = 0; j< MAX_OBSTACLES; j++) {
            if (!obstacles[j].active) {
                obstacles[j].x = LEFT_TRACK_BASE+ offset + 4 + (rand() % (TRACK_WIDTH - 8));
                obstacles[j].page = 0;
                obstacles[j].active = 1;
                break;
            }
        }
    }
		//Impressão dos obstáculos na pista
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            for (col = 0; col < OBSTACLE_WIDTH; col++) {
                int x = obstacles[i].x + col;
                if (x < 64) { CS1 = 0; CS2 = 1; }
                else { x -= 64; CS1 = 1; CS2 = 0; }
                
                GLCD_Write(0xB8 | obstacles[i].page, 1);
                GLCD_Write(0x40 | x, 1);
                GLCD_Write(enemy_car[col], 0);
            }
            if (++obstacles[i].page > 7) {
                obstacles[i].active = 0;
            }
        }
    }
}

int scanKey() {
		//Botões para movimentos laterais 
    LINHA1 = 0;
    if (!COL1) return -4; //Linha 1, Coluna 1
    if (!COL2) return 4; //Linha 1, Coluna 2
    LINHA1 = 1;
    return 0;
}

bit check_collision(int car_left) {
    unsigned char i;
    // Verifica colisão com obstáculos
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && obstacles[i].page >= 7) {
            if (!(obstacles[i].x + OBSTACLE_WIDTH < car_left || obstacles[i].x > car_left + CAR_WIDTH))
                return 1;
        }
    }
    // Verifica colisão com as paredes
    return (car_left < LEFT_TRACK_BASE + track_offsets[7] || 
            car_left + CAR_WIDTH > RIGHT_TRACK_BASE + track_offsets[7]);
}

void Draw_Car(char seta) {
	int car_left;
	unsigned char i;
    center_car += seta;
    car_left = center_car - (CAR_WIDTH / 2);
    
    
    // Desenha o carro quando está na esquerda
    if (car_left + CAR_WIDTH <= 64) {
        CS1 = 0; CS2 = 1;
        GLCD_Write(0xB8 | 7, 1);
        GLCD_Write(0x40 | car_left, 1);
        for (i = 0; i < CAR_WIDTH; i++) GLCD_Write(car[i], 0);
    } 
		// Desenha o carro quando está na direita
    else if (car_left >= 64) {
        CS1 = 1; CS2 = 0;
        GLCD_Write(0xB8 | 7, 1);
        GLCD_Write(0x40 | (car_left - 64), 1);
        for (i = 0; i < CAR_WIDTH; i++) GLCD_Write(car[i], 0);
    }
    else {
        // Carro dividido entre as telas
        CS1 = 0; CS2 = 1;
        GLCD_Write(0xB8 | 7, 1);
        GLCD_Write(0x40 | car_left, 1);
        for (i = 0; i < 64 - car_left; i++) GLCD_Write(car[i], 0);
        
        CS1 = 1; CS2 = 0;
        GLCD_Write(0xB8 | 7, 1);
        GLCD_Write(0x40, 1);
        for (; i < CAR_WIDTH; i++) GLCD_Write(car[i], 0);
    }
    
    colisao = check_collision(car_left);
}

const unsigned int pot10[] = {1, 10, 100, 1000, 10000,100000};

void main() {
	
	int col;
	int num;
	int digito;
	int temp;
	int i;
	GLCD_Init();
	GLCD_ClearAll();
	
	//Tela Inicial
	CS1 = 0; CS2=1;
	GLCD_Write(0xB8+3,1);
	GLCD_Write(0x40,1);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[18][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[19][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[16][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[13][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[15][col],0);
	GLCD_Write(0xB8+4,1);
	GLCD_Write(0x40,1);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[13][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[12][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[17][col],0);
    delay(2000);
		
		//Loop principal
    while (!colisao) {
        GLCD_ClearAll();
        Gerar_Curva();
				Generate_Obstacles();
        seta = scanKey();
        Draw_Car(seta);
        delay(300);
        score++;
    }
		
		//Tela após batida
 while (1) {
	GLCD_ClearAll();
	CS1 = 0; CS2 = 1;
	GLCD_Write(0xB8 | 4,1);
	GLCD_Write(0x40 | 30,1);
	 	for (col = 0; col < 5; col++)
				GLCD_Write(font[18][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[13][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[16][col],0);
	for (col = 0; col < 5; col++)
				GLCD_Write(font[17][col],0);
	 for (col = 0; col < 5; col++)
				GLCD_Write(font[14][col],0);
	 for (col = 0; col < 5; col++)
				GLCD_Write(font[11][col],0);
		num = score;
		temp = num;
		digito = 0;
		while (temp > 0) {
			temp = temp / 10;
			digito++;
		}

		CS1 = 0; CS2 = 1;
		GLCD_Write(0xB8 |  5,1);

		for (i = 0; i < digito; i++) {
			GLCD_Write(0x40 |  30 + i * 6,1);
			for (col = 0; col < 5; col++)
				GLCD_Write(font[1 + (num / pot10[digito - 1 - i])][col],0);
			num = num % pot10[digito - 1 - i];
		}
delay(1000);
		GLCD_ClearAll();
	}
}