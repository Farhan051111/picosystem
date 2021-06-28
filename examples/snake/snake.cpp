#include <string.h>
#include <math.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include "random.hpp"
#include "picosystem.hpp"
#include "pico/stdlib.h"

using namespace picosystem;

// Some handy constants for movement directions and "stopped" state
const Vec2 DIRECTION_UP(0, -1);
const Vec2 DIRECTION_DOWN(0, 1);
const Vec2 DIRECTION_LEFT(-1, 0);
const Vec2 DIRECTION_RIGHT(1, 0);
const Vec2 DIRECTION_STOP(0, 0);

const uint8_t SCALE = 4;

Rect game_bounds = Rect(Point(0, 0), Size(120 / SCALE, 120 / SCALE));

enum INPUT {
    DPAD = 0,
    BUTTONS = 1,
};

struct Player {
    Vec2 direction;
    INPUT input;
    Pen color;
    Point start;
    uint32_t score;
    std::vector<Point> *snake;

    Player() = default;
    Player(INPUT input, Pen color, Point start) : direction(DIRECTION_STOP), input(input), color(color), start(start), score(0) {
        snake = new std::vector<Point>;
        snake->reserve(1000);
        reset();
    };

    void handle_input() {
        if(input == DPAD) {
            // Movement is easy. You can't go back on yourself.
            if(direction.x == 0) {
                if(pressed(dpad::RIGHT)) direction = DIRECTION_RIGHT;
                if(pressed(dpad::LEFT)) direction = DIRECTION_LEFT;
            }
            if(direction.y == 0) {
                if(pressed(dpad::DOWN)) direction = DIRECTION_DOWN;
                if(pressed(dpad::UP)) direction = DIRECTION_UP;
            }
        } else {
            if(direction.x == 0) {
                if(pressed(button::A)) direction = DIRECTION_RIGHT;
                if(pressed(button::Y)) direction = DIRECTION_LEFT;
            }
            if(direction.y == 0) {
                if(pressed(button::B)) direction = DIRECTION_DOWN;
                if(pressed(button::X)) direction = DIRECTION_UP;
            }     
        }
    }
    
    void reset() {
        snake->clear();
        snake->emplace_back(start);
        direction = DIRECTION_STOP;
    }

    void draw() {
        screen.pen = color;
        for(auto segment : *snake) {
            screen.rectangle(Rect(segment * SCALE, Size(SCALE, SCALE)));
        }

        screen.pen = Pen(255, 255, 255);
        if(input == DPAD) {
            screen.text(std::to_string(score), 0, 0);
        } else {
            screen.text(std::to_string(score), 108, 0);
        }
    }

    void update(Player *opponent, Point *apple) {
        if(direction == DIRECTION_STOP) return;

        Point head = snake->back() + direction;
        for(auto segment : *snake) {
            // If the head x/y coordinates match any body/segment
            // coordinates then we've collided with ourselves. BAD LUCK!
            if(head == segment){
                reset();
                return;
            }
        }
        for(auto segment : *opponent->snake) {
            // If the head x/y coordinates match any body/segment
            // coordinates then we've collided with the other player. OWEEE!
            if(head == segment){
                reset();
                return;
            } 
        }
        // Add the new head to our snake's body so it grows
        // in the direction of movement.
        snake->emplace_back(head);

        if(head.x == apple->x && head.y == apple->y) {
            score += 1;
            Point new_apple = get_random_point(game_bounds.size());
            apple->y = new_apple.y;
            apple->x = new_apple.x;
        } else {
            //  If we go out of bounds BAD LUCK!
            // We can check this by seeing if our head is within the game bounds.
            if(!game_bounds.contains(head)) {
                reset();
                return;
            }
            // If we haven't eaten an apple then the snake doesn't get any bigger
            // erase the tail... this means erasing the front of our std::vector
            // which is A BAD THING, but for the sake of snake... it's fiiiinnnee!
            snake->erase(snake->begin());
        }
    }
};

Player *player1;
Player *player2;

uint32_t last_millis = 0;
Point apple;

uint32_t millis() {
    return to_us_since_boot(get_absolute_time()) / 1000;
}

int main() {
  random_reset();
  apple = get_random_point(game_bounds.size());
  player1 = new Player(DPAD, Pen(255, 200, 200), game_bounds.center() - Point(8, 0));
  player2 = new Player(BUTTONS, Pen(200, 200, 255), game_bounds.center() + Point(8, 0));

  init_picosystem();
  set_backlight(255);

  while(true) {
    player1->handle_input();
    player2->handle_input();

    if(millis() - last_millis >= 100) {
        player1->update(player2, &apple);
        player2->update(player1, &apple);
        last_millis = millis();
    }
    screen.pen = Pen(0, 20, 0);
    screen.clear();

    player1->draw();
    player2->draw();

    screen.pen = Pen(0, 255, 0);
    screen.rectangle(Rect(apple * SCALE, Size(SCALE, SCALE)));

    flip();
  }
}
