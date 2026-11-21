#pragma once

// My Products: every StreamUP product in the current scene collection in one
// list, each with a way into its controls (see core/product-registry.hpp).
namespace StreamUP {
namespace MyProductsWindow {

// Opens (or raises) the window. Safe from any thread.
void Show();

} // namespace MyProductsWindow
} // namespace StreamUP
