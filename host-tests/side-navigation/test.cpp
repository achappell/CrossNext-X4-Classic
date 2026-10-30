#include <FreeInkUICore.h>

#include <cstdio>

namespace ui = freeink::ui;
static int checks = 0, failures = 0;
static void check(bool value, const char* message) {
  ++checks;
  if (!value) {
    ++failures;
    std::printf("FAIL: %s\n", message);
  }
}
class FocusTarget final : public ui::DrawTarget {
 public:
  int strokes = 0;
  ui::Rect last{};
  ui::Color color = ui::Color::White;
  ui::Size measureText(ui::FontId, const char*, ui::TextStyle) const override { return {0, 10}; }
  int16_t lineHeight(ui::FontId) const override { return 10; }
  void fill(ui::Rect, ui::Paint, uint8_t, uint8_t) override {}
  void stroke(ui::Rect rect, ui::Paint paint, uint8_t, uint8_t, uint8_t) override {
    ++strokes;
    last = rect;
    color = paint.color;
  }
  void line(ui::Point, ui::Point, uint8_t, ui::Paint) override {}
  void triangle(ui::Point, ui::Point, ui::Point, ui::Paint) override {}
  void text(ui::Rect, const char*, ui::TextStyle) override {}
  void bitmap(ui::Rect, ui::BitmapRef, ui::BitmapMode, ui::Paint, ui::Rotation) override {}
};
int main() {
  ui::InteractionBuffer<5> controls;
  controls.addInteraction({{10, 100, 200, 50}, 1, 11, ui::InputTouch});
  controls.addInteraction({{10, 160, 200, 50}, 1, 12, ui::InputTouch, ui::StateDisabled});
  controls.addInteraction({{10, 220, 200, 50}, 1, 13, ui::InputTouch});
  controls.addInteraction({{10, 300, 50, 40}, 2, -1, ui::InputTouch | ui::InputPrev});
  controls.addInteraction({{160, 300, 50, 40}, 2, 1, ui::InputTouch | ui::InputNext});
  ui::Rect rect;
  check(!controls.hasButtonFocus() && !controls.buttonFocusRect(rect), "Touch-only screen starts without cursor");
  ui::InputSnapshot input{};
  input.confirm = true;
  check(!controls.route(input), "Confirm cannot activate an unselected item");
  input = {};
  input.focusNext = true;
  check(!controls.route(input) && controls.focusedIndex() == 0, "First press selects first row without activating it");
  check(controls.buttonFocusRect(rect) && rect.x == 10 && rect.y == 100, "Focused geometry is exact");
  FocusTarget focusPaint;
  controls.paintButtonFocus(focusPaint);
  check(focusPaint.strokes == 0, "Existing non-Classic ordinary control rendering is unchanged");
  controls.setExplicitCoordinateNavigation(true);
  controls.paintButtonFocus(focusPaint);
  check(focusPaint.strokes > 0 && focusPaint.color == ui::Color::Black,
        "Classic ordinary chrome paints a visible contrasting focus outline");
  check(focusPaint.last.x >= rect.x && focusPaint.last.y >= rect.y &&
            focusPaint.last.right() <= rect.right() && focusPaint.last.bottom() <= rect.bottom(),
        "Chrome focus remains inside the selected actionable control");
  controls.setFocusedIndex(1);
  focusPaint.strokes = 0;
  controls.paintButtonFocus(focusPaint);
  check(focusPaint.strokes == 0, "Disabled controls never paint actionable focus");
  controls.setFocusedIndex(0);
  controls.setExplicitCoordinateNavigation(false);
  input = {};
  input.confirm = true;
  auto event = controls.route(input);
  check(event.action == 1 && event.value == 11, "Focused touch row can be confirmed");
  input = {};
  input.focusNext = true;
  controls.route(input);
  check(controls.focusedIndex() == 2, "Disabled rows are skipped");
  input = {};
  input.focusPrev = true;
  controls.route(input);
  check(controls.focusedIndex() == 0, "Previous row skips disabled row");
  input = {};
  input.next = true;
  event = controls.route(input);
  check(event.action == 2 && event.value == 1, "Held next uses the visible pager's action");
  input = {};
  input.prev = true;
  event = controls.route(input);
  check(event.action == 2 && event.value == -1, "Held previous uses the visible pager's action");
  input = {};
  input.touchReleased = true;
  input.touchX = 15;
  input.touchY = 225;
  event = controls.route(input);
  check(event.value == 13 && !controls.hasButtonFocus(), "Touch activation restores cursor-free appearance");
  for (int n = 0; n < 100; ++n) {
    input = {};
    input.focusNext = true;
    controls.route(input);
    check(controls.focusedIndex() != 1, "Repeated navigation never selects disabled entry");
  }
  controls.clear();
  input = {};
  input.confirm = true;
  check(!controls.route(input), "Stale focus cannot activate after menu change");
  input = {};
  input.next = true;
  check(!controls.route(input), "An absent pager cannot invent a next action");
  ui::InteractionBuffer<2> board;
  board.addInteraction({{20, 40, 90, 90}, 7, -1, ui::InputTouch});
  board.addInteraction({{20, 40, 90, 90},
                        ui::NO_ACTION,
                        0,
                        ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                        ui::StateNormal,
                        0,
                        3,
                        3});
  board.setFocusedIndex(1);
  check(!board.buttonFocusRect(rect), "Programmatic selection does not show button cursor");
  for (int cell = 0; cell < 9; ++cell) {
    input = {};
    input.focusNext = true;
    // Enter the coordinate grid from the preceding regular control.
    if (!cell) board.setFocusedIndex(0);
    const auto previous = board.focusPosition();
    board.route(input);
    check(previous != board.focusPosition(), "Every cell movement has a distinct repaint position");
    check(board.buttonFocusRect(rect) && rect.x == 20 + (cell % 3) * 30 && rect.y == 40 + (cell / 3) * 30,
          "Grid focus maps to cell centre in drawn coordinates");
    input = {};
    input.confirm = true;
    event = board.route(input);
    check(event.action == 7 && event.value == -1, "Coordinate confirmation reuses original touch action");
    // The flag intentionally remains on across confirmations and cell changes.
    if (cell < 8) {
      input = {};
      input.touchReleased = true;
      input.touchX = -1;
      input.touchY = -1;
      board.route(input);
    }
  }
  board.setExplicitCoordinateNavigation(true);
  board.setFocusedIndex(-1);
  input = {};
  input.focusNext = true;
  board.route(input);
  board.route(input);
  check(board.coordinateFocused() && !board.coordinateEditing(), "Classic chrome focuses the coordinate editor");
  const auto chromePosition = board.focusPosition();
  input = {};
  input.confirm = true;
  check(!board.route(input) && board.coordinateEditing(), "Entering editor does not activate its first cell");
  check(board.focusPosition() != chromePosition, "Entering editor requests a distinct repaint");
  check(board.moveCoordinateCursor(1, 0), "Classic horizontal cursor moves without dispatch");
  check(board.moveCoordinateCursor(0, 1), "Classic vertical cursor moves without paging");
  check(board.buttonFocusRect(rect) && rect.x == 50 && rect.y == 70, "Both axes select the expected visible cell");
  board.moveCoordinateCursor(1, 0);
  const auto rightEdge = board.focusPosition();
  check(!board.moveCoordinateCursor(1, 0) && board.focusPosition() == rightEdge,
        "A horizontal grid edge cannot wrap into the next row");
  board.moveCoordinateCursor(-1, 0);
  board.moveCoordinateCursor(-1, 0);
  const auto leftEdge = board.focusPosition();
  check(!board.moveCoordinateCursor(-1, 0) && board.focusPosition() == leftEdge,
        "A horizontal grid edge cannot wrap into the previous row");
  board.moveCoordinateCursor(1, 0);
  event = board.route(input);
  check(event.action == 7, "Subsequent Confirm commits through the existing cell action");
  check(!board.moveCoordinateCursor(0, 3), "Out-of-bounds cursor movement preserves the selected cell");
  check(board.leaveCoordinateEditor() && !board.coordinateEditing(), "Back leaves editing without activating anything");
  check(!board.route(input) && board.coordinateEditing(), "Re-entering requires its own nonmutating Confirm");
  board.leaveCoordinateEditor();
  input = {};
  input.focusPrev = true;
  board.route(input);
  check(!board.coordinateFocused(), "Classic chrome traverses a grid once, not every cell");
  ui::InteractionBuffer<3> publishedBoard;
  publishedBoard.setExplicitCoordinateNavigation(true);
  publishedBoard.beginPublishCycle();
  publishedBoard.addInteraction({{20, 40, 90, 90}, 70, 17, ui::InputTouch});
  publishedBoard.addInteraction({{20, 40, 90, 90}, ui::NO_ACTION, 0,
                                ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                                ui::StateNormal, 0, 3, 3});
  publishedBoard.publish();
  input = {};
  input.focusNext = true;
  publishedBoard.routePublished(input);
  publishedBoard.routePublished(input);
  input = {};
  input.confirm = true;
  check(!publishedBoard.routePublished(input) && publishedBoard.coordinateEditing(),
        "Published coordinate target enters editing without dispatch");
  publishedBoard.beginPublishCycle();
  publishedBoard.clear();
  publishedBoard.addInteraction({{200, 200, 30, 30}, 71, 99, ui::InputTouch});
  check(!publishedBoard.coordinateFocused() && !publishedBoard.coordinateEditing(),
        "Public coordinate queries retain the unpublished building context");
  event = publishedBoard.routePublished(input);
  check(event.action == 70 && event.value == 17,
        "A dissimilar unpublished table cannot cancel published coordinate editing");
  event = publishedBoard.routePublished(input);
  check(event.action == 70 && event.value == 17,
        "Repeated published confirmation dispatches only the published cell action");
  publishedBoard.route({});
  check(!publishedBoard.routePublished(input),
        "Routing a non-coordinate building table cancels editing despite the published coordinate target");
  event = publishedBoard.routePublished(input);
  check(event.action == 70 && event.value == 17,
        "Published editing resumes only after its own nonmutating entry confirmation");
  ui::InteractionBuffer<4> keyboard;
  keyboard.addInteraction({{0, 0, 90, 60}, 9, 0, ui::InputTouch | ui::InputLongPress | ui::InputPointerOnly});
  keyboard.addInteraction({{0, 0, 90, 30}, 0, 0, ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                           ui::StateNormal, 0, 3, 1});
  keyboard.addInteraction({{5, 30, 90, 30}, 0, 0, ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                           ui::StateNormal, 0, 3, 1});
  keyboard.addInteraction({{100, 30, 30, 30}, 0, 0, ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                           ui::StateNormal, 0, 1, 1});
  keyboard.setExplicitCoordinateNavigation(true);
  input = {};
  input.focusNext = true;
  keyboard.route(input);
  input = {};
  input.confirm = true;
  keyboard.route(input);
  keyboard.moveCoordinateCursor(1, 0);
  check(keyboard.moveCoordinateCursor(0, 1), "Keyboard vertical navigation crosses bounded row grids");
  const auto secondRow = keyboard.data()[2].rect;
  check(keyboard.buttonFocusRect(rect) && rect.x == secondRow.x + secondRow.width / 3 && rect.y == secondRow.y,
        "Keyboard preserves horizontal column");
  keyboard.moveCoordinateCursor(-1, 0);
  check(!keyboard.moveCoordinateCursor(-1, 0) && keyboard.buttonFocusRect(rect) && rect.y == secondRow.y,
        "An offset keyboard row cannot leak sideways into another row");
  keyboard.moveCoordinateCursor(2, 0);
  check(keyboard.moveCoordinateCursor(1, 0) && !keyboard.moveCoordinateCursor(1, 0),
        "A bounded keyboard row still reaches its adjacent Enter/Erase grid");
  keyboard.moveCoordinateCursor(-1, 0);
  input.longPress = true;
  event = keyboard.route(input);
  check(event.action == 9 && event.longPress, "Existing held action remains distinct from a short confirmation");
  ui::InteractionBuffer<3> duplicateActions;
  duplicateActions.addInteraction({{10, 10, 80, 40}, 90, 0, ui::InputTouch});
  duplicateActions.addInteraction({{10, 100, 80, 40}, 90, 0, ui::InputTouch});
  duplicateActions.addInteraction({{10, 200, 80, 40}, 91, 0, ui::InputTouch});
  duplicateActions.setExplicitCoordinateNavigation(true);
  input = {};
  input.focusNext = true;
  duplicateActions.route(input);
  duplicateActions.route(input);
  check(duplicateActions.buttonFocusRect(rect) && rect.y == 100,
        "Duplicate semantic actions retain their distinct focused surface");
  duplicateActions.route(input);
  input = {};
  input.confirm = true;
  check(duplicateActions.route(input).action == 91,
        "A header/body duplicate action cannot trap navigation before later controls");
  const auto selectedChoice = static_cast<ui::State>(ui::StateSelected | ui::StateChecked);
  check(duplicateActions.buttonSelected(90, 0, true) && !duplicateActions.buttonSelected(91, 0, false) &&
            (duplicateActions.stateFor(90, 0, selectedChoice) & selectedChoice) == selectedChoice,
        "Classic focus preserves semantic selection and checked state");
  duplicateActions.setExplicitCoordinateNavigation(false);
  check(duplicateActions.buttonSelected(91, 0, false) &&
            !(duplicateActions.stateFor(90, 0, selectedChoice) & selectedChoice),
        "Non-Classic focus styling keeps its existing selection behavior");
  ui::InteractionBuffer<3> slidingTiles;
  slidingTiles.addInteraction({{0, 0, 30, 30}, 92, 5, ui::InputTouch});
  slidingTiles.addInteraction({{0, 40, 30, 30}, 92, 7, ui::InputTouch});
  slidingTiles.addInteraction({{0, 100, 80, 30}, 93, 0, ui::InputTouch});
  slidingTiles.setExplicitCoordinateNavigation(true);
  slidingTiles.coordinateAction(92);
  input = {};
  input.focusNext = true;
  slidingTiles.route(input);
  input = {};
  input.confirm = true;
  check(!slidingTiles.route(input) && slidingTiles.coordinateEditing(),
        "A legal sliding tile enters editing without moving");
  check(slidingTiles.route(input).value == 5, "A second Confirm chooses the legal tile");
  slidingTiles.clear();
  slidingTiles.addInteraction({{0, 0, 30, 30}, 92, 2, ui::InputTouch});
  slidingTiles.addInteraction({{0, 40, 30, 30}, 92, 7, ui::InputTouch});
  slidingTiles.addInteraction({{0, 100, 80, 30}, 93, 0, ui::InputTouch});
  slidingTiles.coordinateAction(92);
  check(slidingTiles.coordinateEditing(), "A moved tile does not silently leave coordinate editing");
  check(slidingTiles.route(input).value == 2, "The rebuilt editor chooses a remaining legal target");
  check(slidingTiles.leaveCoordinateEditor(), "Back still leaves the rebuilt editor before the view");
  input = {};
  input.focusPrev = true;
  slidingTiles.route(input);
  input = {};
  input.confirm = true;
  check(slidingTiles.route(input).action == 93, "Leaving a rebuilt editor restores chrome traversal");
  ui::InteractionBuffer<1> column;
  column.addInteraction({{0, 0, 30, 90}, 94, 0,
                         ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                         ui::StateNormal, 0, 1, 3});
  column.setExplicitCoordinateNavigation(true);
  input = {};
  input.focusNext = true;
  column.route(input);
  input = {};
  input.confirm = true;
  column.route(input);
  check(column.moveCoordinateCursor(1, 0) && column.buttonFocusRect(rect) && rect.y == 30,
        "A one-column grid retains bottom-pair linear traversal");
  check(column.moveCoordinateCursor(-1, 0) && !column.moveCoordinateCursor(-1, 0),
        "One-column linear traversal remains bounded");
  std::printf("Side navigation: %d checks, %d failures; buffer bytes=%zu\n", checks, failures, sizeof(controls));
  return failures ? 1 : 0;
}
