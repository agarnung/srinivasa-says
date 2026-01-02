#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define F_CPU 16000000UL
#define PRESCALER 64

// Frecuencias para los dígitos 0-9 (Hz)
const uint16_t NOTES[10] = {523, 587, 659, 698, 784, 880, 932, 1046, 1175, 1319};

const uint8_t PI_DIGITS[] = {3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3,2,3,8,4,6,2,6,4,3,3,8,3,2,7,9,5,0,2,8,8,4,1,9,7,1,6,9,3,9,9,3,7,5,1,0,5,8,2,0,9,7,4,9,4,4,5,9,2,3,0,7,8,1,6,4,0,6,2,8,6,2,0,8,9};
#define PI_SIZE (sizeof(PI_DIGITS)/sizeof(PI_DIGITS[0]))

// Variables globales volátiles
volatile uint8_t button_pressed = 10; // Valor inicial inválido
volatile uint8_t button_event = 0;
uint8_t current_position = 0;

// Prototipos de funciones
void set_frequency(uint16_t freq);
void handle_error(uint8_t pressed);
void handle_correct(uint8_t pressed);

// Configuración del Timer1 y buzzer
void set_frequency(uint16_t frequency) {
    // Deshabilitar interrupciones temporalmente
    cli();
    
    if(frequency == 0 || (PINB & (1 << PB2))) { // Si PB2 está en HIGH (mute)
        TCCR1B &= ~((1 << CS12)|(1 << CS11)|(1 << CS10)); // Detener timer
        PORTB &= ~(1 << PB4); // PB4 en LOW
    } else {
        TCCR1A = 0; // Modo normal
        TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // CTC, prescaler 64
        OCR1A = (F_CPU / PRESCALER / (2 * frequency)) - 1; 
        TCNT1 = 0; // Reiniciar contador
        TIMSK1 |= (1 << OCIE1A); // Habilitar interrupción
    }
    
    sei(); // Rehabilitar interrupciones
}

// ISR para generación de tono
ISR(TIMER1_COMPA_vect) {
    PORTB ^= (1 << PB4); // Toggle buzzer
    PORTC ^= (1 << PC2); // Toggle LED de depuración (conectar LED a PC2)
}

// Interrupción para PD0-PD7 (dígitos 0-7)
ISR(PCINT2_vect) {
    uint8_t i; // Variable declarada al inicio para C89
    for(i = 0; i <= 7; i++) {
        if(!(PIND & (1 << i))) {
            button_pressed = i;
            button_event = 1;
            break;
        }
    }
}

// Interrupción para PB0-PB1 (dígitos 8-9)
ISR(PCINT0_vect) {
    if(!(PINB & (1 << PB0))) {
        button_pressed = 8;
        button_event = 1;
    } 
    else if(!(PINB & (1 << PB1))) {
        button_pressed = 9;
        button_event = 1;
    }
}

// Configuración inicial de pines
void setup_pins() {
    DDRD = 0x00; // PORTD como entradas
    PORTD = 0xFF; // Habilitar pull-ups en PD0-PD7
    
    DDRB |= (1 << PB4); // PB4 como salida (buzzer)
    
    DDRB &= ~((1 << PB0) | (1 << PB1)); // Configurar PB0 y PB1 como entradas
    PORTB |= (1 << PB0) | (1 << PB1); // Activar pull-ups en PB0 y PB1
    DDRB &= ~(1 << PB2); // Configurar PB2 como entrada sin pull-up

    DDRC |= (1 << PC0)|(1 << PC1); // LEDs verde (PC0) y rojo (PC1)
    PORTC &= ~((1 << PC0)|(1 << PC1)); // LEDs inicialmente apagados
}

// Configurar interrupciones
void setup_interrupts() {
    PCICR |= (1 << PCIE2)|(1 << PCIE0); // Habilitar PCINT2 y PCINT0
    PCMSK2 |= 0xFF; // PD0-PD7
    PCMSK0 |= (1 << PCINT0)|(1 << PCINT1); // PB0 y PB1
    sei(); // Habilitar interrupciones globales
}

// Manejar error
void handle_error(uint8_t pressed) {
    PORTC |= (1 << PC1); // Encender LED rojo
    
    // Reproducir tono de error solo si no está muteado
    if(!(PINB & (1 << PB2))) { // Si PB2 está en LOW (mute desactivado)
        set_frequency(800);  // Primer tono alto (200ms)
        _delay_ms(200);
        set_frequency(400);  // Segundo tono bajo (300ms)
        _delay_ms(450);
        set_frequency(0);    // Silenciar
    } 
    else {
        // Si está muteado, mantener LED por 500ms mínimo
        _delay_ms(500);
    }

    // Esperar hasta que se suelte el botón
    while(1) {
        if( ( (pressed <= 7) && (PIND & (1 << pressed)) ) ||  // Botones 0-7 (PD0-PD7)
            ( (pressed > 7) && (PINB & (1 << (pressed - 8))) ) ) { // Botones 8-9 (PB0-PB1)
            break;
        }
        _delay_ms(10);
    }

    _delay_ms(500); // Garantiza 500ms total desde el inicio
    PORTC &= ~(1 << PC1); // Apagar LED
}

// Manejar acierto
void handle_correct(uint8_t pressed) {
    PORTC |= (1 << PC0);
    if(!(PINB & (1 << PB2))) { // Si PB2 está en LOW (mute desactivado)
        set_frequency(NOTES[pressed]);
    }
    while(1) {
        if( (pressed <= 7 && (PIND & (1 << pressed))) || 
            (pressed > 7 && (PINB & (1 << (pressed - 8)))) ) { 
            break;
        }
        _delay_ms(10);
    }
    set_frequency(0);
    PORTC &= ~(1 << PC0);
}

int main(void) {
    setup_pins();
    setup_interrupts();
    sei(); // Asegurar interrupciones habilitadas
    
    while(1) {
        if(button_event) {
            cli();
            uint8_t pressed = button_pressed;
            button_event = 0;
            sei();
            
            // Anti-rebote (20ms + verificación)
            _delay_ms(20);
            uint8_t still_pressed = (pressed <= 7) ? 
                !(PIND & (1 << pressed)) : 
                !(PINB & (1 << (pressed-8)));
            
            if(!still_pressed) continue;

            // Lógica principal
            if(pressed == PI_DIGITS[current_position]) {
                handle_correct(pressed);
                current_position = (current_position + 1) % PI_SIZE;
            } else {
                handle_error(pressed);
                current_position = 0;
            }
        }
    }
    return 0;
}