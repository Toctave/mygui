#pragma once

// As defined in USB HID usage tables
#define FOR_ALL_KEY_INDICES(ITEM)                                              \
    ITEM(0, UNKNOWN)                                                           \
    ITEM(1, ERROR_ROLLOVER)                                                    \
    ITEM(2, POST_FAIL)                                                         \
    ITEM(3, ERROR_UNDEFINED)                                                   \
    ITEM(4, A)                                                                 \
    ITEM(5, B)                                                                 \
    ITEM(6, C)                                                                 \
    ITEM(7, D)                                                                 \
    ITEM(8, E)                                                                 \
    ITEM(9, F)                                                                 \
    ITEM(10, G)                                                                \
    ITEM(11, H)                                                                \
    ITEM(12, I)                                                                \
    ITEM(13, J)                                                                \
    ITEM(14, K)                                                                \
    ITEM(15, L)                                                                \
    ITEM(16, M)                                                                \
    ITEM(17, N)                                                                \
    ITEM(18, O)                                                                \
    ITEM(19, P)                                                                \
    ITEM(20, Q)                                                                \
    ITEM(21, R)                                                                \
    ITEM(22, S)                                                                \
    ITEM(23, T)                                                                \
    ITEM(24, U)                                                                \
    ITEM(25, V)                                                                \
    ITEM(26, W)                                                                \
    ITEM(27, X)                                                                \
    ITEM(28, Y)                                                                \
    ITEM(29, Z)                                                                \
    ITEM(30, 1)                                                                \
    ITEM(31, 2)                                                                \
    ITEM(32, 3)                                                                \
    ITEM(33, 4)                                                                \
    ITEM(34, 5)                                                                \
    ITEM(35, 6)                                                                \
    ITEM(36, 7)                                                                \
    ITEM(37, 8)                                                                \
    ITEM(38, 9)                                                                \
    ITEM(39, 0)                                                                \
    ITEM(40, ENTER)                                                            \
    ITEM(41, ESCAPE)                                                           \
    ITEM(42, BACKSPACE)                                                        \
    ITEM(43, TAB)                                                              \
    ITEM(44, SPACE)                                                            \
    ITEM(45, MINUS)                                                            \
    ITEM(46, EQUALS)                                                           \
    ITEM(47, LEFT_BRACKET)                                                     \
    ITEM(48, RIGHT_BRACKET)                                                    \
    ITEM(49, BACKSLASH)                                                        \
    ITEM(50, HASH_NONUS)                                                       \
    ITEM(51, SEMICOLON)                                                        \
    ITEM(52, APOSTROPHE)                                                       \
    ITEM(53, GRAVE_ACCENT)                                                     \
    ITEM(54, COMMA)                                                            \
    ITEM(55, PERIOD)                                                           \
    ITEM(56, SLASH)                                                            \
    ITEM(57, CAPS_LOCK)                                                        \
    ITEM(58, F1)                                                               \
    ITEM(59, F2)                                                               \
    ITEM(60, F3)                                                               \
    ITEM(61, F4)                                                               \
    ITEM(62, F5)                                                               \
    ITEM(63, F6)                                                               \
    ITEM(64, F7)                                                               \
    ITEM(65, F8)                                                               \
    ITEM(66, F9)                                                               \
    ITEM(67, F10)                                                              \
    ITEM(68, F11)                                                              \
    ITEM(69, F12)                                                              \
    ITEM(70, PRINT_SCREEN)                                                     \
    ITEM(71, SCROLL_LOCK)                                                      \
    ITEM(72, PAUSE)                                                            \
    ITEM(73, INSERT)                                                           \
    ITEM(74, HOME)                                                             \
    ITEM(75, PAGE_UP)                                                          \
    ITEM(76, DELETE)                                                           \
    ITEM(77, END)                                                              \
    ITEM(78, PAGE_DOWN)                                                        \
    ITEM(79, RIGHT_ARROW)                                                      \
    ITEM(80, LEFT_ARROW)                                                       \
    ITEM(81, DOWN_ARROW)                                                       \
    ITEM(82, UP_ARROW)                                                         \
    ITEM(83, NUM_LOCK)                                                         \
    ITEM(84, KP_SLASH)                                                         \
    ITEM(85, KP_STAR)                                                          \
    ITEM(86, KP_MINUS)                                                         \
    ITEM(87, KP_PLUS)                                                          \
    ITEM(88, KP_ENTER)                                                         \
    ITEM(89, KP_1)                                                             \
    ITEM(90, KP_2)                                                             \
    ITEM(91, KP_3)                                                             \
    ITEM(92, KP_4)                                                             \
    ITEM(93, KP_5)                                                             \
    ITEM(94, KP_6)                                                             \
    ITEM(95, KP_7)                                                             \
    ITEM(96, KP_8)                                                             \
    ITEM(97, KP_9)                                                             \
    ITEM(98, KP_0)                                                             \
    ITEM(99, KP_PERIOD)                                                        \
    ITEM(100, ANTISLASH_NONUS)                                                 \
    ITEM(101, APPLICATION)                                                     \
    ITEM(102, POWER)                                                           \
    ITEM(103, KP_EQUALS)                                                       \
    ITEM(104, F13)                                                             \
    ITEM(105, F14)                                                             \
    ITEM(106, F15)                                                             \
    ITEM(107, F16)                                                             \
    ITEM(108, F17)                                                             \
    ITEM(109, F18)                                                             \
    ITEM(110, F19)                                                             \
    ITEM(111, F20)                                                             \
    ITEM(112, F21)                                                             \
    ITEM(113, F22)                                                             \
    ITEM(114, F23)                                                             \
    ITEM(115, F24)                                                             \
    ITEM(116, EXECUTE)                                                         \
    ITEM(117, HELP)                                                            \
    ITEM(118, MENU)                                                            \
    ITEM(119, SELECT)                                                          \
    ITEM(120, STOP)                                                            \
    ITEM(121, AGAIN)                                                           \
    ITEM(122, UNDO)                                                            \
    ITEM(123, CUT)                                                             \
    ITEM(124, COPY)                                                            \
    ITEM(125, PASTE)                                                           \
    ITEM(126, FIND)                                                            \
    ITEM(127, MUTE)                                                            \
    ITEM(128, VOLUME_UP)                                                       \
    ITEM(129, VOLUME_DOWN)                                                     \
    ITEM(130, LOCKING_CAPS_LOCK)                                               \
    ITEM(131, LOCKING_NUM_LOCK)                                                \
    ITEM(132, LOCKING_SCROLL_LOCK)                                             \
    ITEM(133, KEYPAD_COMMA)                                                    \
    ITEM(134, KEYPAD_EQUALS_2)                                                 \
    ITEM(135, INTERNATIONAL1)                                                  \
    ITEM(136, INTERNATIONAL2)                                                  \
    ITEM(137, INTERNATIONAL3)                                                  \
    ITEM(138, INTERNATIONAL4)                                                  \
    ITEM(139, INTERNATIONAL5)                                                  \
    ITEM(140, INTERNATIONAL6)                                                  \
    ITEM(141, INTERNATIONAL7)                                                  \
    ITEM(142, INTERNATIONAL8)                                                  \
    ITEM(143, INTERNATIONAL9)                                                  \
    ITEM(144, LANG1)                                                           \
    ITEM(145, LANG2)                                                           \
    ITEM(146, LANG3)                                                           \
    ITEM(147, LANG4)                                                           \
    ITEM(148, LANG5)                                                           \
    ITEM(149, LANG6)                                                           \
    ITEM(150, LANG7)                                                           \
    ITEM(151, LANG8)                                                           \
    ITEM(152, LANG9)                                                           \
    ITEM(153, ALTERNATE_ERASE)                                                 \
    ITEM(154, SYSREQ)                                                          \
    ITEM(155, CANCEL)                                                          \
    ITEM(156, CLEAR)                                                           \
    ITEM(157, PRIOR)                                                           \
    ITEM(158, RETURN)                                                          \
    ITEM(159, SEPARATOR)                                                       \
    ITEM(160, OUT)                                                             \
    ITEM(161, OPER)                                                            \
    ITEM(162, CLEAR_AGAIN)                                                     \
    ITEM(163, CRSEL_PROPS)                                                     \
    ITEM(164, EXSEL)                                                           \
    ITEM(165, RESERVED_165)                                                    \
    ITEM(166, RESERVED_166)                                                    \
    ITEM(167, RESERVED_167)                                                    \
    ITEM(168, RESERVED_168)                                                    \
    ITEM(169, RESERVED_169)                                                    \
    ITEM(170, RESERVED_170)                                                    \
    ITEM(171, RESERVED_171)                                                    \
    ITEM(172, RESERVED_172)                                                    \
    ITEM(173, RESERVED_173)                                                    \
    ITEM(174, RESERVED_174)                                                    \
    ITEM(175, RESERVED_175)                                                    \
    ITEM(176, KP_00)                                                           \
    ITEM(177, KP_000)                                                          \
    ITEM(178, THOUSANDS_SEPARATOR)                                             \
    ITEM(179, DECIMAL_SEPARATOR)                                               \
    ITEM(180, CURRENCY_UNIT)                                                   \
    ITEM(181, CURRENCY_SUB_UNIT)                                               \
    ITEM(182, KP_LEFT_PARENTHESIS)                                             \
    ITEM(183, KP_RIGHT_PARENTHESIS)                                            \
    ITEM(184, KP_LEFT_BRACE)                                                   \
    ITEM(185, KP_RIGHT_BRACE)                                                  \
    ITEM(186, KP_TAB)                                                          \
    ITEM(187, KP_BACKSPACE)                                                    \
    ITEM(188, KP_A)                                                            \
    ITEM(189, KP_B)                                                            \
    ITEM(190, KP_C)                                                            \
    ITEM(191, KP_D)                                                            \
    ITEM(192, KP_E)                                                            \
    ITEM(193, KP_F)                                                            \
    ITEM(194, KP_XOR)                                                          \
    ITEM(195, KP_CARET)                                                        \
    ITEM(196, KP_PERCENT)                                                      \
    ITEM(197, KP_LESS_THAN)                                                    \
    ITEM(198, KP_GREATER_THAN)                                                 \
    ITEM(199, KP_AMPERSAND)                                                    \
    ITEM(200, KP_DOUBLE_AMPERSAND)                                             \
    ITEM(201, KP_PIPE)                                                         \
    ITEM(202, KP_DOUBLE_PIPE)                                                  \
    ITEM(203, KP_COLON)                                                        \
    ITEM(204, KP_HASH)                                                         \
    ITEM(205, KP_SPACE)                                                        \
    ITEM(206, KP_AT)                                                           \
    ITEM(207, KP_EXCLAMATION_MARK)                                             \
    ITEM(208, KP_MEMORY_STORE)                                                 \
    ITEM(209, KP_MEMORY_RECALL)                                                \
    ITEM(210, KP_MEMORY_CLEAR)                                                 \
    ITEM(211, KP_MEMORY_ADD)                                                   \
    ITEM(212, KP_MEMORY_SUBTRACT)                                              \
    ITEM(213, KP_MEMORY_MULTIPLY)                                              \
    ITEM(214, KP_MEMORY_DIVIDE)                                                \
    ITEM(215, KP_PLUS_OR_MINUS)                                                \
    ITEM(216, KP_CLEAR)                                                        \
    ITEM(217, KP_CLEAR_ENTRY)                                                  \
    ITEM(218, KP_BINARY)                                                       \
    ITEM(219, KP_OCTAL)                                                        \
    ITEM(220, KP_DECIMAL)                                                      \
    /* from now on, not in the HID table */                                    \
    ITEM(221, LEFT_CONTROL)                                                    \
    ITEM(222, RIGHT_CONTROL)                                                   \
    ITEM(223, LEFT_SHIFT)                                                      \
    ITEM(224, RIGHT_SHIFT)                                                     \
    ITEM(225, LEFT_ALT)                                                        \
    ITEM(226, RIGHT_ALT)                                                       \
    ITEM(227, SUPER)

#define KEY_COUNT 228

#define DO_KEY_INDEX_ENUM(index, name) KEY_##name,

#define DO_KEY_NAME(index, name) [index] = #name,

enum key_index_e
{
    FOR_ALL_KEY_INDICES(DO_KEY_INDEX_ENUM)
};

extern const char* KEY_NAMES[KEY_COUNT];
