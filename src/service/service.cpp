#include "fairy_viewer_service.hpp"

int main()
{
    service::FairyViewerService service(1600, 900, 3, 60);
    service.Run();
    return 0;
}