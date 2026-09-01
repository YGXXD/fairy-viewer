#include "fairy_viewer_app.hpp"

int main(int argc, char* argv[])
{
    app::FairyViewerApp app("fairy-viewer", 1920, 1080, 1600, 900);
    app.Run();
    return 0;
}