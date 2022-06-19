//! VERY slim SDL2 bindings.

use std::{
    ffi::CStr,
    fmt::{self, Display, Formatter},
    os::raw::c_int,
    ptr::NonNull,
};

#[allow(non_camel_case_types)]
mod ffi {
    use std::os::raw::{c_char, c_int};

    pub type SDL_bool = c_int;
    pub type SDL_JoystickID = i32;

    #[repr(C)]
    pub struct SDL_GameController {
        _opaque: [u8; 0],
    }

    type SDL_GameControllerAxis = super::GameControllerAxis;
    type SDL_GameControllerButton = super::GameControllerButton;

    extern "C" {
        pub fn SDL_Init(_: u32) -> c_int;
        pub fn SDL_Quit();

        pub fn SDL_GetError() -> *const c_char;

        pub fn SDL_IsGameController(_: c_int) -> SDL_bool;
        pub fn SDL_GameControllerOpen(_: c_int) -> *mut SDL_GameController;
        pub fn SDL_GameControllerClose(_: *mut SDL_GameController);
        pub fn SDL_GameControllerUpdate();
        pub fn SDL_GameControllerGetAxis(
            _: *mut SDL_GameController,
            _: SDL_GameControllerAxis,
        ) -> i16;
        pub fn SDL_GameControllerGetButton(
            _: *mut SDL_GameController,
            _: SDL_GameControllerButton,
        ) -> SDL_bool;
    }
}

/// SDL context.
pub struct Sdl;

impl Sdl {
    /// Initialize SDL.
    pub fn init(flags: Init) -> Result<Self, Error> {
        resi(unsafe { ffi::SDL_Init(flags.0) })?;
        Ok(Sdl)
    }

    /// Returns whether or not the specified device index is a game controller.
    pub fn is_game_controller(&self, device_index: usize) -> bool {
        unsafe { ffi::SDL_IsGameController(device_index as _) != 0 }
    }

    /// Opens a new game controller handle.
    pub fn game_controller_open(&self, device_index: usize) -> Result<GameController, Error> {
        let handle = resp(unsafe { ffi::SDL_GameControllerOpen(device_index as _) })?;
        Ok(GameController { handle })
    }

    /// Updates game controller state.
    pub fn game_controller_update(&self) {
        unsafe { ffi::SDL_GameControllerUpdate() };
    }
}

impl Drop for Sdl {
    fn drop(&mut self) {
        unsafe { ffi::SDL_Quit() };
    }
}

/// SDL initialization flags.
pub struct Init(u32);

impl Init {
    /// Initialize game controller API.
    pub const GAMECONTROLLER: Self = Init(0x00002000);
}

/// A joystick ID.
#[derive(Eq, Hash, PartialEq)]
pub struct JoystickId(ffi::SDL_JoystickID);

/// A game controller.
pub struct GameController {
    handle: NonNull<ffi::SDL_GameController>,
}

impl GameController {
    /// Gets the value of a game controller axis.
    pub fn axis(&self, axis: GameControllerAxis) -> i16 {
        unsafe { ffi::SDL_GameControllerGetAxis(self.handle.as_ptr(), axis) }
    }

    /// Gets the pressed state of a game controller button.
    pub fn button(&self, button: GameControllerButton) -> bool {
        unsafe { ffi::SDL_GameControllerGetButton(self.handle.as_ptr(), button) != 0 }
    }
}

impl Drop for GameController {
    fn drop(&mut self) {
        unsafe { ffi::SDL_GameControllerClose(self.handle.as_ptr()) };
    }
}

/// A game controller axis.
#[repr(C)]
pub enum GameControllerAxis {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    TriggerLeft = 4,
    TriggerRight = 5,
}

/// A game controller button.
#[repr(C)]
pub enum GameControllerButton {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    Back = 4,
    Guide = 5,
    Start = 6,
    LeftStick = 7,
    RightStick = 8,
    LeftShoulder = 9,
    RightShoulder = 10,
    DpadUp = 11,
    DpadDown = 12,
    DpadLeft = 13,
    DpadRight = 14,
    _Misc1 = 15,
    _Paddle1 = 16,
    _Paddle2 = 17,
    _Paddle3 = 18,
    _Paddle4 = 19,
    _Touchpad = 20,
}

/// An SDL error.
#[derive(Debug)]
pub struct Error(String);

impl Error {
    /// Gets the last SDL error that was set.
    fn last() -> Self {
        let message = unsafe {
            CStr::from_ptr(ffi::SDL_GetError())
                .to_string_lossy()
                .into_owned()
        };
        Self(message)
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for Error {}

fn resi(i: c_int) -> Result<(), Error> {
    if i == 0 {
        Ok(())
    } else {
        Err(Error::last())
    }
}

fn resp<T>(p: *mut T) -> Result<NonNull<T>, Error> {
    NonNull::new(p).ok_or_else(Error::last)
}
