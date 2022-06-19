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

    pub struct SDL_GameController {
        _opaque: [u8; 0],
    }

    pub struct SDL_Joystick {
        _opaque: [u8; 0],
    }

    extern "C" {
        pub fn SDL_Init(_: u32) -> c_int;
        pub fn SDL_Quit();

        pub fn SDL_GetError() -> *const c_char;

        pub fn SDL_NumJoysticks() -> c_int;
        pub fn SDL_JoystickGetDeviceInstanceID(_: c_int) -> SDL_JoystickID;
        pub fn SDL_JoystickInstanceID(_: *mut SDL_Joystick) -> SDL_JoystickID;

        pub fn SDL_IsGameController(_: c_int) -> SDL_bool;
        pub fn SDL_GameControllerFromInstanceID(_: SDL_JoystickID) -> *mut SDL_GameController;
        pub fn SDL_GameControllerGetJoystick(_: *mut SDL_GameController) -> *mut SDL_Joystick;
        pub fn SDL_GameControllerOpen(_: c_int) -> *mut SDL_GameController;
        pub fn SDL_GameControllerClose(_: *mut SDL_GameController);
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

impl Drop for GameController {
    fn drop(&mut self) {
        unsafe { ffi::SDL_GameControllerClose(self.handle.as_ptr()) };
    }
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
