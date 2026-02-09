import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {

  // =============================================
  // LEARN — Conceptual understanding for everyone
  // =============================================
  learnSidebar: [
    'learn/index',
    'learn/why-simplex-embedded',
    {
      type: 'category',
      label: 'Architecture',
      collapsed: false,
      items: [
        'learn/architecture/protocol-stack',
        'learn/architecture/encryption-layers',
        'learn/architecture/network-topology',
      ],
    },
    {
      type: 'category',
      label: 'Core Concepts',
      collapsed: false,
      items: [
        'learn/concepts/simplex-queues',
        'learn/concepts/duplex-connections',
        'learn/concepts/identity-model',
        'learn/concepts/message-lifecycle',
      ],
    },
    {
      type: 'category',
      label: 'Reference',
      collapsed: true,
      items: [
        'reference/glossary',
        'reference/faq',
        'reference/changelog',
        'reference/roadmap',
      ],
    },
  ],

  // =============================================
  // SPEC — Formal protocol reference
  // =============================================
  specSidebar: [
    'spec/index',
    'spec/terminology',
    {
      type: 'category',
      label: 'SMP Protocol',
      collapsed: false,
      items: [
        'spec/smp/index',
        'spec/smp/transport-tls',
        'spec/smp/wire-format',
        {
          type: 'category',
          label: 'Queue Lifecycle',
          items: [
            'spec/smp/cmd-queue-creation',
            'spec/smp/cmd-queue-securing',
            'spec/smp/cmd-queue-management',
          ],
        },
        {
          type: 'category',
          label: 'Message Exchange',
          items: [
            'spec/smp/cmd-message-send',
            'spec/smp/cmd-message-receive',
          ],
        },
        {
          type: 'category',
          label: 'Extensions',
          collapsed: true,
          items: [
            'spec/smp/cmd-notifications',
            'spec/smp/cmd-private-routing',
          ],
        },
        'spec/smp/error-handling',
        'spec/smp/message-padding',
      ],
    },
    {
      type: 'category',
      label: 'Cryptography',
      collapsed: false,
      items: [
        'spec/crypto/x3dh',
        'spec/crypto/double-ratchet',
        'spec/crypto/post-quantum-kem',
        'spec/crypto/nacl-layers',
        'spec/crypto/primitives',
      ],
    },
    {
      type: 'category',
      label: 'Agent Protocol',
      collapsed: false,
      items: [
        'spec/agent/index',
        'spec/agent/duplex-connections',
        'spec/agent/message-integrity',
      ],
    },
    {
      type: 'category',
      label: 'XFTP Protocol',
      collapsed: true,
      items: [
        'spec/xftp/index',
        'spec/xftp/encryption',
        'spec/xftp/commands',
      ],
    },
    {
      type: 'category',
      label: 'Chat Protocol',
      collapsed: true,
      items: [
        'spec/chat/message-types',
        'spec/chat/groups',
        'spec/chat/calls',
        'spec/chat/files',
        'spec/chat/contacts',
      ],
    },
    'spec/ntf-protocol',
    'spec/xrcp-protocol',
    'spec/security-considerations',
    'spec/test-vectors',
  ],

  // =============================================
  // BUILD — C/ESP32 developer guide
  // =============================================
  implementSidebar: [
    'implement/index',
    {
      type: 'category',
      label: 'Getting Started',
      collapsed: false,
      items: [
        'implement/getting-started/prerequisites',
        'implement/getting-started/building',
        'implement/getting-started/project-structure',
      ],
    },
    {
      type: 'category',
      label: 'ESP32 Platform',
      collapsed: false,
      items: [
        'implement/esp32/memory-management',
        'implement/esp32/flash-storage',
        'implement/esp32/power-management',
      ],
    },
    {
      type: 'category',
      label: 'Transport Layer',
      collapsed: false,
      items: [
        'implement/transport/tls-mbedtls',
        'implement/transport/tcp-connections',
        'implement/transport/block-framing',
      ],
    },
    {
      type: 'category',
      label: 'Cryptography in C',
      collapsed: false,
      items: [
        'implement/crypto/library-selection',
        'implement/crypto/asymmetric-ops',
        'implement/crypto/symmetric-ops',
        'implement/crypto/sntrup761',
        'implement/crypto/key-storage',
      ],
    },
    {
      type: 'category',
      label: 'SMP Client',
      collapsed: false,
      items: [
        'implement/smp/connection-lifecycle',
        'implement/smp/command-encoding',
        'implement/smp/queue-state-machine',
        'implement/smp/padding',
      ],
    },
    {
      type: 'category',
      label: 'Agent Layer',
      collapsed: false,
      items: [
        'implement/agent/duplex-manager',
        'implement/agent/double-ratchet-c',
        'implement/agent/x3dh-impl',
        'implement/agent/integrity-chain',
      ],
    },
    {
      type: 'category',
      label: 'Feature Modules',
      collapsed: false,
      items: [
        'implement/features/text-messaging',
        'implement/features/delivery-receipts',
        'implement/features/file-transfer',
        'implement/features/groups',
      ],
    },
    {
      type: 'category',
      label: 'Testing & Validation',
      collapsed: true,
      items: [
        'implement/testing/test-vectors',
        'implement/testing/interop',
        'implement/testing/hardware',
      ],
    },
  ],

  // =============================================
  // HARDWARE — Dedicated hardware documentation
  // =============================================
  hardwareSidebar: [
    'hardware/HARDWARE_OVERVIEW',
    'hardware/HAL_ARCHITECTURE',
    'hardware/HARDWARE_TIERS',
    'hardware/COMPONENT_SELECTION',
    'hardware/PCB_DESIGN',
    'hardware/ENCLOSURE_DESIGN',
    'hardware/SECURITY_ARCHITECTURE',
  ],

  // =============================================
  // SECURITY — Security model and analysis
  // =============================================
  securitySidebar: [
    'security/index',
    'SECURITY_MODEL',
    'security/threat-model',
    'security/encryption-deep-dive',
    'security/hardware-security',
    'security/audit-log',
  ],

  // =============================================
  // WHY SIMPLEGO? — Comparisons and rationale
  // =============================================
  whySimpleGoSidebar: [
    'why-simplego/index',
    {
      type: 'category',
      label: 'Protocol Comparisons',
      collapsed: false,
      items: [
        'SIMPLEX_VS_MATRIX',
        'SIMPLEGO_VS_GRAPHENEOS',
        'why-simplego/vs-signal',
        'why-simplego/vs-briar',
      ],
    },
    {
      type: 'category',
      label: 'Design Decisions',
      collapsed: false,
      items: [
        'why-simplego/why-own-hardware',
        'why-simplego/why-esp32',
      ],
    },
  ],

  // =============================================
  // ANALYSIS — Protocol analysis sessions
  // =============================================
  analysisSidebar: [
    'protocol-analysis/SIMPLEX_PROTOCOL_INDEX',
    'protocol-analysis/SIMPLEX_STATUS',
    'protocol-analysis/QUICK_REFERENCE',
    'protocol-analysis/BUG_TRACKER',
    {
      type: 'category',
      label: 'Sessions',
      collapsed: false,
      items: [
        'protocol-analysis/PART1_INTRO_SESSIONS_1-2',
        'protocol-analysis/PART2_SESSIONS_3-4',
        'protocol-analysis/PART3_SESSIONS_5-6',
        'protocol-analysis/PART4_SESSION_7',
        'protocol-analysis/PART5_SESSION_8_BREAKTHROUGH',
        'protocol-analysis/PART6_SESSION_9',
        'protocol-analysis/PART7_SESSION_10',
        'protocol-analysis/PART8_SESSION_11',
        'protocol-analysis/PART9_SESSION_12',
        'protocol-analysis/PART10_SESSION_13',
        'protocol-analysis/PART11_SESSION_14',
        'protocol-analysis/PART12_SESSION_15',
        'protocol-analysis/PART13_SESSION_16',
        'protocol-analysis/PART14_SESSION_17',
        'protocol-analysis/PART15_SESSION_18',
        'protocol-analysis/PART16_SESSION_19',
        'protocol-analysis/PART17_SESSION_20',
        'protocol-analysis/PART18_SESSION_21',
        'protocol-analysis/PART19_SESSION_22',
        'protocol-analysis/PART20_SESSION_23',
      ],
    },
    'protocol-analysis/README',
  ],

  // =============================================
  // PROJECT — Software docs + Releases
  // =============================================
  projectSidebar: [
    {
      type: 'category',
      label: 'Software',
      collapsed: false,
      items: [
        'ARCHITECTURE',
        'BUILD_SYSTEM',
        'DEVELOPMENT',
        'TECHNICAL',
        'CRYPTO',
        'PROTOCOL',
        'WIRE_FORMAT',
        'BUGS',
        'DEVNOTES',
      ],
    },
    {
      type: 'category',
      label: 'Releases',
      collapsed: false,
      items: [
        'releases/index',
        'releases/v0-1-16-alpha',
        'releases/v0-1-15-alpha',
        'releases/v0-1-14-alpha',
      ],
    },
    {
      type: 'category',
      label: 'About',
      collapsed: true,
      items: [
        'ADDING_NEW_DEVICE',
        'SPONSORS',
        'DISCLAIMER',
        'TRADEMARK',
        'LEGAL',
      ],
    },
  ],
};

export default sidebars;

