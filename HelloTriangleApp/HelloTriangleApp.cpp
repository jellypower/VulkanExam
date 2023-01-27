#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <algorithm>
#include<string>


VkResult CreateDeubgUtilMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

 
class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;


    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    VkDevice device;
    VkQueue graphicsQueue;
    // 큐는 기본적으로 logical device를 생성하면 같이 만들어지지만
    // 생성만 되고 우리가 이에 대한 handle을 아직 가지고 있지 않기 때문에
    // 이를 명시적으로 가져와 줘야 한다.
    VkQueue presentQueue;


#ifdef NDEBUG
    const bool enableValidationlayers = false;
#else
    const bool enableValidationlayers = true;
#endif

    bool checkValidationLayerSupport() {
        uint32_t layerCount;

        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());


        std::cout << "\n\n";

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {

                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }


            }

            if (!layerFound) {
                return false;
            }

        }

        return true;

    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationlayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;

    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

private:

    void initWindow() {

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    }

#pragma region initVulkan
    void initVulkan() {
        createInstance();
        setupDebugMessanger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
    }

    void createImageViews() {
        // imageView는 말 그대로 image에 대한 view이다.
        // 이미지 뷰는 어떻게 이미지에 접근하고 어느 이미지에 접근할지를 정의해준다.
        // 예를 들어 밉맵없는 2D뎁스맵 텍스쳐로 다뤄져야 하는 경우처럼 말이다.

        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;


            // component field는 swizzling이 가능하게 해준다.
            //swizzling: 그래픽스에서 swizzling은 다른 벡터의 구성 요소를 임의로 재배열하고 결합하여 벡터를 구성하는 기능
            // 가령, A = {1,2,3,4}일때 B = A.wwxy 면 B={4,4,1,2} 이다.
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // subresourceRange필드는 이미지를 사용하는 목표, 이미지의 어느 부분이 접근 가능한지 등을 기술한다. 
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            // stereographic 3D application을 만들기 위해선 multiple layer의 swap chain을 생성해야 한다.
            // 그러면 왼쪽, 오른쪽 눈에 대응되는 multiple image view를 만들어낼 수 있다.

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
            
        }

        // 지금 설정까지 하면 imageView를 texture로 만들기엔 충분하다.
        // 하지만 아직 redner target으로 만들려면 아직이다.

    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        // surface는 window창과 대응되는 개념
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = choosSwapExtent(swapChainSupport.capabilities);


        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;


        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // 각각의 이미지를 구성하는 레이어의 개수, stereoscopy 3D 기술을 쓰지 않으면 항상 1이다.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        // imsageUsage는 스왑체인에 무슨 종류의 operation을 사용할지 고르는 것이다.
        // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT는 스왑체인의 이미지에 바로 그리는 것을 의미하며 color attachment라고 한다.
        // 포스트 프로세싱같은 작동을 위해 별도의 이미지에 이미지를 렌더링 하는 것도 가능하며 이를 위해선
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT를 대신 사용하면 된다.

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndiecs[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // 명시적으로 ownership을 transfer하지 않아도 다른 큐에서 사용 가능하다.
            // 해당 옵션을 위해선 사전에 어느 큐 패밀리 ownership이 공유될지를 미리 설정해야 한다.
            // 해당 모드는 일반적으로 두개 이상의 큐 패밀리를 가진 디바이스에서 필요로된다.
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndiecs;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // 큐 패밀리가 가진 이미지는 명시적으로 transfer돼야 다른 큐에서 사용 가능하게 한다.
            // 성능적으로 효율이 좋다.
            createInfo.queueFamilyIndexCount = 0; //optional
            createInfo.pQueueFamilyIndices = nullptr; //optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 스왑체인의 이미지에 특별한 transform이 들어가야 하는지를 알려줌.(ex. 시계방향으로 90도 회전)
        // 필요 없으면 currentTransform으로 설정

        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // 다른window system에서 window랑 blending하는데 알파채널이 사용되는지를 특정함. 대부분 무시할거기 때문에 위와 같이 설정

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        // 다른 윈도우 창에 가려서 clipping된 픽셀의 색은 신경쓰지 않는 모드
        // 가려진 픽셀을 읽어 결과를 예측하는 경우가 아니면 활성화하여 성능 향상 가능

        createInfo.oldSwapchain = VK_NULL_HANDLE;
        // 화면이 resize되거나 하는 등의 이유로 swap chain이 비활성되거나 최적화되지 못하는 경우가 있음
        // 이런 경우에 스왑체인은 이전 버전의 스왑체인을 참고해 새로이 만들어져야 함.
        // 일단 지금은 스왑체인 하나만 만드는 형태이기때문에 스킵


        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        // 차례로 디바이스, createInfo, 전용 allocator, swapchin
        // 전용 allocator는 본인이 원하는 대로 swapchain의 create함수를 정의 가능

        
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount!= 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.graphicsFamily.value() };
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationlayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }
        // 이전 vulkan의 구현에서는 instance와 validation layer간의 구분을 뒀었다.
        // 그런데, RayTracing으로 인해 이제 그것은 더 이상 구분이 되지 않는다.
        // 그 말인 즉슨, enabledLayerCount와 ppEnabledLayerNames는 현재 무시되고 있는 상황이다.
        // 그러나, 예전 구현 표준에 맞춰 이것을 어떻게든 초기화 하는 것은 아직도 추천되고 있는 상황이다.

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);


    }

    void createInstance() {

        if (enableValidationlayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available");
        }


        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 236);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 236);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        // applicaton info를 만든다. 이는 기술적으로는 설정하지 않아도 되지만
        // driver에게 최적화에 관련된 유용한 정보들을 넘겨준다.

        VkInstanceCreateInfo createInfo{}; // createInfo는 필수이다.
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo; //appInfo는 createInfo에 담아서 전달된다.

        
        auto extensions = getRequiredExtensions();

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        // glfw를 포함해 해당 어플에 필요로되는 익스텐션이 무엇이 있는지 목록을 만들어
        // 해당 데이터를 포함해 vkInstance를 만든다.



        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationlayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }


        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }



    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessanger() {
        if (!enableValidationlayers) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDeubgUtilMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
        
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

      

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        

        for (const auto& device : devices) {

            if (isDeviceSuitable(device)) {
                physicalDevice = device;

                VkPhysicalDeviceProperties deviceProp;
                vkGetPhysicalDeviceProperties(physicalDevice, &deviceProp);
                std::cout<< "Selected device: " << deviceProp.deviceName << "\n";

                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

    }



    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        // drawing command와 presentation의 큐 패밀리는 겹치지 않기 때문에 
        // 별도의 기능을 하는 새로운 큐를 만들 필요가 있다.

        bool isComplete() {

            return graphicsFamily.has_value() && presentFamily.has_value();
        }
        //Queue Family에 대한 설명
        //https://lifeisforu.tistory.com/404
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;


        for (const auto& queueFamily : queueFamilies) {

            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }


            if (indices.isComplete()) {
                break;
            }



            i++;
        }


        return indices;

    }


    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);


        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }


    // 각각의 VkSurfaceFormatKHR 요소들은 format과 colorSpace 멤버를 포함한다.
    // format 은 컬러 채널과 타입을 특정한다. VK_FORMAT_B8G8R8A8_SRGB라면 8bit uint로 RGBA를 저장한다는 뜻이다. 
    // colorSpace는 국제 규격인 SRGB color space가 지원되는지를 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 플래그로 확인 할 수 있다.
    // 해당 플래그는 올드버전에서 VK_COLORSPACE_SRGB_NONLINEAR_KHR 라고 불렸다.
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {

        for (const auto& availableFormats : availableFormats) {

            if (availableFormats.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormats.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormats;

            }
        }

        return availableFormats[0];

    }
    // SRGB와 linear RGB의 차이 => 인같은 어두운 환경을 인지하는 능력이 더 뛰어나기 때문에 선형으로 빛의 밝기를 표현하면
    // 구분하지도 못할 밝은 영역의 빛의 intensity는 촘촘하게 구성되고 구분하기 용이한 어두운 영역의 빛의 intensity는 rought하게 된다
    // 이를 위해 비트의 구성을 변형하여 빛의 세기를 표현하는 것이 SRGB의 요지이다.



    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        //presentation mode는 총 4개가 존재한다.
        // VK_PRESENT_MODE_IMMEDIATE_KHR: app에서 계산된 이미지가 바로 스크린에 전달됨. 티어링이 생길 수 있음
        // VK_PRESENT_MODE_FIFO_KHR: fifo형태의 큐에 이미지를 쌓아놓음.
        //          큐가 가득차면 프로그램이 기다림. v-sync와 가장 유사함. 화면이 refresh되는 순간을 vertical blank라고 부름
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR: 위의 모드와 유사. 다른점이라면 어플의 계산이 늦어지고 마지막 vertical blank에서
        //          큐가 비어있으면 다음 vertical blank를 기다려서 동기화 하는 것이 아닌 현재 렌더링하고 있는 이미지가 바로
        //          그려짐. visible tearing을 만들어냄
        // VK_PRESENT_MODE_MAILBOX_KHR: 두 번째 모드와 유사. 차이점으론 큐가 가득 찼을 때 블락을 거는 것이 아닌 
        //          큐의 마지막 이미지가 덮어쓰기됨. 티어링 방지가 가능하고 렌더링을 빨리 할 수 있어 수직동기화보다 latency
        //          이슈가 적게 일어남. 흔히 트리플 버퍼링이라고 불림. 근데 실제 버퍼가 3개 있다고 프레임속도의 제한이 풀리는 것은 아님


        // VK_PRESENT_MODE_MAILBOX_KHR 모드는 에너지 효율을 생각하지 않으면 가장 좋은 옵션임
        // VK_PRESENT_MODE_FIFO_KHR 모드는 모바일 디바이스처럼 에너지 효율이 필요로 될 때 사용하기 좋음
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choosSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // SwapChain의 extent는 swapchain의 resolution을 의미하며 이는 보통 DPI가 높은 모니터를 사용하지 않는 이상
    // 모니터의 resolution과 똑같음. 
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            // glfw는 window 사이즈를 잴 때, screen coordinate랑 pixel 두 가지 unit을 제공함

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);
            
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);

            return actualExtent;

        }

        
    }


#pragma endregion

    void mainLoop() {

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }

    }

    void cleanup() {

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
            // window의 image와 다르게 imageView는 명시적을 우리가 직접 생성했기 때문에 직접 지워줘야함
            

        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);

        vkDestroyDevice(device, nullptr);

        if (enableValidationlayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }


};

int main()
{
    HelloTriangleApplication app;

    try {
        app.run();
    }
    catch (const std::exception e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
