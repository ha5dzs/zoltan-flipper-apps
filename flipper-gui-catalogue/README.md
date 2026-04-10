# Flipper GUI Samples

I would probably include something grammatically shiny such as this app is 'a comprehensive demonstration application for Flipper Zero GUI components. This app showcases nearly all available GUI modules with working examples and proper scene management'.

## Why

This application, in its entirety is the total opposite of the description above. In reality, this application is dysfunctional, in the sense that it is not supposed to do anything. It is just a collection of working code of some of the GUI features and systems the Flipper uses. If you install this, you will see some unamusing menus and GUI elements. There is more stuff in the debug messages in the command line interface, and the code is _kind of_ made to be easily readable. Sadly, these things are complex, there are intertwined callback functions everywhere, so it is really difficult to see how it all goes together.

I just made this so I would understand how GUI is being handled in the Flipper - I was OK with ViewPort, but all the rage is at ViewDispatcher and SceneManager. [Derek Jamison's tutorials](https://github.com/jamisonderek/flipper-zero-tutorials/) were my starting point, but I felt that it always goes into something specific and not generic. Later-on, I had to come to the conclusion that wanting something universal that works with the GUI in the Flipper Zero is not easily possible, if at all. This is because there are intertwined callback functions everywhere, and it is my impression that it is way too ambitious for what it is - a self-contained microcontroller ecosystem. Don't get me wrong, I can see the brilliance and the talent that went into this, but this came at a prince of it REALLY not being DIY friendly. And I am saying this with about 20 years of low-level embedded development work behind my back.

On that note, I now have some GenAI pipeline that did most of the heavy lifting for me, so from now on, I now know how to cast my spells to that I would get the necessary magic done. And this is a good thing, because this allows me to concentrate on the finer things, the things _I actually want to do_ with this device - so this is kind of like a result of learning to use a swiss army knife, so now I can go and do things with said swiss army knife - if you get my analogy.

## ...and the real reason why

Anyway, I originally didn't want to post it, but I made the mistake of posting about this on LinkedIn. Now, being an interdisciplinary scientist-type person, I regularly get accused of making stuff up in order to falsely impress people, and even the suspicion of this have been grounds for dismissal by some arrogant and perhaps jealous people in charge. So, here is the evidence of me not making stuff up, said stuff working and running without crashing and burning, so LinkedIn keyboard warriors, choke on it! :)

## What's used in the app?

These are the GUI elements

| Flipper-specific GUI terminology | What it does |
|---------|-------------|
| **SubMenu** | Scrollable menu with callback-based selection |
| **Widget** | Display text, icons, and scrollable content |
| **DialogEx** | Confirmation dialogs with Left/Center/Right buttons |
| **Popup** | Temporary messages with auto-dismiss timeout |
| **ByteInput** | Hexadecimal input (32-bit buffer) |
| **TextInput** | Single-line text input with callback |
| **NumberInput** | Numeric input with min/max range |
| **Menu** | Classic Flipper list menu with icons |
| **ButtonMenu** | Menu items as buttons with press callbacks |
| **Scene Manager** | Navigation between different app scenes |
| **View Dispatcher** | Switches between views for each scene |

## Navigation

Button release and long-press event types are NOT used here, but can be implemented separately.

- **OK** - Select menu item / Click button or fieldt
- **Back** - Return to previous menu / Exit app
- **Arrow Keys** - Navigate / Scroll / Adjust

## Scenes (i.e. the one I ended up sweating blood with)

There are a number of different callback functions here. One false move with a pointer and the entire thing crashes. Or of it doesn't crash, it will lock up due to some unhandled event and you'll have to reset manually (by pressing and holding 'Left' and 'Back' - you will thank me later for this!)

| Scene | Description |
|-------|-------------|
| **Main Menu** | Entry point with list of all demos |
| **Dialog Demo** | Multi-button confirmation dialog |
| **Popup Demo** | Auto-dismissing message (3 second timeout) |
| **ByteInput Demo** | 4-byte hex input (0x12 0x34 0x56 0x78 default) |
| **Widget Demo** | Content area using Widget module |
| **Text Input** | Single-line text entry |
| **About** | Application info with scrolling text |
| **Loading Demo** | Brief loading indicator (2 seconds) |
| **Number Input** | Numeric entry (0-100 range, default 42) |
| **Menu Demo** | Classic list menu with options - this really should be the main menu, but it's funnier this way |
| **Button Menu** | Button-based menu with custom callbacks |

## Code Structure

```
flipper-gui-catalogue/
├── gui_examples.c      # Main entry point, view dispatcher, scene setup
├── gui_examples.h      # Header with scene enum and GlobalVariableStruct
├── helper_functions.c  # Title screen draw callback
├── scenes/             # Scene implementations
│   ├── main_menu.c
│   ├── dialog_demo.c
│   ├── popup_demo.c
│   ├── byte_input_demo.c
│   ├── text_input_demo.c
│   ├── number_input_demo.c
│   ├── menu_demo.c
│   ├── button_menu_demo.c
│   ├── about_scene.c
│   ├── loading_screen.c
│   └── *.h files for all scenes
├── images/             # Image assets (icon, GUI examples screenshot)
├── application.fam     # Application manifest
└── README.md           # This file
```

## Callback Pattern

All scenes follow a consistent callback pattern. This is the PTSD-inducing stuff!

This `GlobalVariableStruct` thing is the main app structure - in the Flipper world, we don't use global variables, because there is FreeRTOS around mucking around in the memory doing other things so you, the user will stay happy - if your application starts playing around in the memory recklessly for no good reason, things will crash and burn. So, by placing your application-specific stuff into this pseudo-global-variable structure and tossing its pointer as the argument, you will have your memory protected by the OS, and you have the additional luxury of being able to directly modify the contents of the strucuture. But the devil is in the details - the pointer is just a 32-bit memory address, and how much space you have actually allocated, is totally up to you. If you find yourself outside of this allocated memory, then it will crash. If you don't cast this pointer properly, then it will crash. If you even THINK about handling this pointer with the appropriate humility it deserves and DEMANDS, it will crash. So, yes, this was and is difficult.

```c
// Result callback - called when user confirms input
static void result_callback(void* context) {
    GlobalVariableStruct* app = context;
    scene_manager_next_scene(app->scene_manager, SceneMain);
}

// Scene entry - configure the component
void scene_<name>_on_enter(void* context) { ... }

// Event handler - process navigation events
bool scene_<name>_on_event(void* context, SceneManagerEvent event) { ... }

// Exit handler - cleanup
void scene_<name>_on_exit(void* context) { ... }
```

## Deploying on device

I included the compiled binary in `dist/gui_examples.fap`. But, is there is a breaking change in the Flipper Zero firmware AGAIN, you have to recompile the code. This is best done with `ufbt`, which is better than `fbt` for application development. It's faster and lighter. You can use the stock `fbt` too, which is included in the device firmware - whichever flavour of it you may prefer.

```bash
# Clean temporary files if concerned about garbage
ufbt -c

# Build the application
ufbt

# Run on connected Flipper Zero
ufbt launch

# Get to the command line interface and start logging for extra entertainment
ufbt cli

>: log debug

```

## Otherwise

- The app displays a title screen for 2 seconds on startup
- All scenes return to the main menu on Back press
- Callbacks use `scene_manager_next_scene()` for navigation
- Resources are properly freed in the exit handlers (not like if I DARE not!)
- View Dispatcher handles all view switching - this was difficult too
- Scene Manager handles event routing and navigation state - most of the time
