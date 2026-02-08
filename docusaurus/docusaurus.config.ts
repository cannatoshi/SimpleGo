import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'SimpleGo Docs',
  tagline: 'Native SimpleX Protocol Implementation for ESP32',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://docs.simplego.dev',
  baseUrl: '/',

  organizationName: 'cannatoshi',
  projectName: 'SimpleGo',
  deploymentBranch: 'gh-pages',
  trailingSlash: false,

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  markdown: {
    format: 'detect',
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          path: '../docs',
          sidebarPath: './sidebars.ts',
          editUrl: 'https://github.com/cannatoshi/SimpleGo/tree/main/docs/',
          routeBasePath: '/',
          exclude: ['**/gfx/**', '**/release-info/**'],
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    image: 'img/simplex-spec-social.png',
    announcementBar: {
      id: 'wip',
      content: '🚧 SimpleGo is under active development. Pages marked 📋 are planned for future releases.',
      isCloseable: true,
    },
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: false,
    },
    docs: {
      sidebar: {
        hideable: true,
        autoCollapseCategories: true,
      },
    },
    navbar: {
      title: 'SimpleGo',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'learnSidebar',
          position: 'left',
          label: 'Learn',
        },
        {
          type: 'docSidebar',
          sidebarId: 'specSidebar',
          position: 'left',
          label: 'Spec',
        },
        {
          type: 'docSidebar',
          sidebarId: 'implementSidebar',
          position: 'left',
          label: 'Build',
        },
        {
          type: 'docSidebar',
          sidebarId: 'hardwareSidebar',
          position: 'left',
          label: 'Hardware',
        },
        {
          type: 'docSidebar',
          sidebarId: 'securitySidebar',
          position: 'left',
          label: 'Security',
        },
        {
          type: 'docSidebar',
          sidebarId: 'whySimpleGoSidebar',
          position: 'left',
          label: 'Why SimpleGo?',
        },
        {
          type: 'docSidebar',
          sidebarId: 'analysisSidebar',
          position: 'left',
          label: 'Analysis',
        },
        {
          type: 'docSidebar',
          sidebarId: 'projectSidebar',
          position: 'left',
          label: 'Project',
        },
        {
          href: 'https://github.com/cannatoshi/SimpleGo',
          label: 'GitHub',
          position: 'right',
        },
        {
          href: 'https://simplex.chat',
          label: 'SimpleX Chat',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Documentation',
          items: [
            { label: 'Learn', to: '/learn' },
            { label: 'Specification', to: '/spec' },
            { label: 'Build Guide', to: '/implement' },
          ],
        },
        {
          title: 'Project',
          items: [
            { label: 'Hardware', to: '/hardware/HARDWARE_OVERVIEW' },
            { label: 'Security', to: '/security' },
            { label: 'Why SimpleGo?', to: '/why-simplego' },
            { label: 'Analysis', to: '/protocol-analysis/SIMPLEX_PROTOCOL_INDEX' },
            { label: 'Roadmap', to: '/reference/roadmap' },
            { label: 'SimpleGo on GitHub', href: 'https://github.com/cannatoshi/SimpleGo' },
          ],
        },
        {
          title: 'Community',
          items: [
            { label: 'SimpleX Chat', href: 'https://simplex.chat' },
            { label: 'S.D - IT and More Systems', href: 'https://it-and-more-systems.de' },
          ],
        },
        {
          title: 'Legal',
          items: [
            { label: 'Impressum', to: '/legal/impressum' },
            { label: 'Privacy Policy', to: '/legal/privacy' },
            { label: 'Spec: CC BY-SA 4.0', href: 'https://creativecommons.org/licenses/by-sa/4.0/' },
            { label: 'Code: AGPL-3.0', href: 'https://github.com/cannatoshi/SimpleGo/blob/main/LICENSE' },
          ],
        },
      ],
      copyright: `© ${new Date().getFullYear()} S.D - IT and More Systems. All rights reserved.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
      additionalLanguages: ['c', 'haskell', 'bash', 'json'],
    },
    tableOfContents: {
      minHeadingLevel: 2,
      maxHeadingLevel: 4,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
